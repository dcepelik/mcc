#include "debug.h"
#include "lexer.h"
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>


static struct tokinfo eol = { .token = TOKEN_EOL };
static struct tokinfo eof = { .token = TOKEN_EOF };

static const char simple_escape_seq[256] = {
	['a'] = '\a',
	['b'] = '\b',
	['f'] = '\f',
	['r'] = '\r',
	['n'] = '\n',
	['v'] = '\v',
	['t'] = '\t',
	['\\'] = '\\',
	['\"'] = '\"',
	['\''] = '\'',
	['\?'] = '\?',
};


inline static bool is_letter(int c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}


inline static bool is_digit(int c)
{
	return c >= '0' && c <= '9';
}


inline static bool is_whitespace(int c)
{
	return c == '\n' || c == '\r' || c == '\t' || c == 0x0C || c == ' ';
}


inline static bool is_hex_digit(int c)
{
	return is_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}


inline static bool is_octal_digit(int c)
{
	return c >= '0' && c <= '7';
}


static int32_t read_octal_number(struct cppfile *file)
{
	uint32_t val = 0;
	int i;

	for (i = 0; i < 3; i++) {
		if (!is_octal_digit(*file->c))
			break;

		val *= 8;
		val += *file->c - '0';

		file->c++;
	}

	if (i == 0) {
		DEBUG_MSG("error: octal digit expected");
	}

	return val;
}


static inline int hex_val(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';

	if (c >= 'a' && c <= 'f')
		return 10 + (c - 'a');

	if (c >= 'A' && c <= 'F')
		return 10 + (c - 'A');

	return -1;
}


static uint32_t read_hex_number(struct cppfile *file, size_t min_len, size_t max_len)
{	
	uint32_t val = 0;
	int i;

	for (i = 0; i < max_len; i++) {
		if (!is_hex_digit(*file->c))
			break;

		val *= 16;
		val += hex_val(*file->c);

		file->c++;
	}

	if (i < min_len)
		DEBUG_MSG("error: hex digit expected");


	return val;
}


static uint32_t read_escape_sequence(struct cppfile *file)
{
	assert(file->c[-1] == '\\');

	int c = *file->c;
	file->c++;

	if (simple_escape_seq[c])
		return simple_escape_seq[c];

	switch (c) {
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
		file->c--;
		return read_octal_number(file);

	case 'x':	
		return read_hex_number(file, 1, 8);

	case 'u':
		return read_hex_number(file, 4, 4);

	case 'U':
		return read_hex_number(file, 8, 8);

	default:
		DEBUG_PRINTF("error: unknown escape sequence: \\%c", *file->c);
		file->c++;
		return -1;
	}
}


struct symbol *lexer_upsert_symbol(struct cppfile *file, char *name)
{
	struct symbol *symbol;
	
	symbol = symtab_search(file->symtab, name);
	if (!symbol) {
		symbol = symtab_insert(file->symtab, name);
		symbol->type = SYMBOL_TYPE_UNKNOWN;
	}

	return symbol;
}


struct tokinfo *lex_name(struct cppfile *file, struct tokinfo *tokinfo)
{
	strbuf_reset(&file->strbuf);

	while (is_letter(*file->c) || is_digit(*file->c) || *file->c == '_' || *file->c == '\\') {
		strbuf_putc(&file->strbuf, *file->c); // TODO UCN support affects this
		file->c++;
	}

	tokinfo->token = TOKEN_NAME;
	tokinfo->symbol = lexer_upsert_symbol(file, strbuf_get_string(&file->strbuf));

	if (!tokinfo->symbol)
		return NULL;

	return tokinfo;
}


static struct tokinfo *lex_char(struct cppfile *file, struct tokinfo *tokinfo)
{
	uint32_t val;
	bool seen_delimiter = false;
	size_t i;

	assert(file->c[-1] == '\'');

	for (i = 0; *file->c != '\0'; i++) {
		if (*file->c == '\'') {
			seen_delimiter = true;
			file->c++;
			break;
		}

		if (*file->c == '\\') {
			file->c++;
			val = read_escape_sequence(file);
		}
		else {
			val = (int)(*file->c);
			file->c++;
		}
	}

	if (i == 0)
		DEBUG_MSG("error: empty character constant");

	if (i > 1)
		DEBUG_MSG("error: multichar character constants are not supported")

	if (!seen_delimiter)
		DEBUG_MSG("error: missing the final \'");

	tokinfo->token = TOKEN_CHAR_CONST;
	tokinfo->value = val;

	return tokinfo;
}


static inline bool lexer_is_eol(struct cppfile *file)
{
	return (file->c - strbuf_get_string(&file->linebuf))
		>= strbuf_strlen(&file->linebuf);
}


struct tokinfo *lex_pp_number(struct cppfile *file, struct tokinfo *tokinfo)
{
	char c;

	strbuf_reset(&file->strbuf);

	while (!lexer_is_eol(file)) {
		if (*file->c == '+' || *file->c == '-') {
			/* assert(not at the BOL) */
			assert(file->c != strbuf_get_string(&file->linebuf));

			c = file->c[-1];
			if (c != 'e' && c != 'E' && c != 'p' && c != 'P') {
				break;
			}
		}
		else if (*file->c == '\\') {
			// process UCN
			file->c++;
			continue;
		}
		else if (!is_digit(*file->c) && !is_letter(*file->c) && *file->c != '.' && *file->c != '_') {
			break;
		}

		strbuf_putc(&file->strbuf, *file->c);
		file->c++;
	}

	tokinfo->token = TOKEN_NUMBER;
	tokinfo->str = strbuf_copy_to_mempool(&file->strbuf, &file->token_data);

	if (!tokinfo->str)
		return NULL;

	return tokinfo;
}


struct tokinfo *lex_string(struct cppfile *file, struct tokinfo *tokinfo)
{
	strbuf_reset(&file->strbuf);

	bool seen_delimiter = false;
	size_t i;

	assert(file->c[-1] == '\"');

	for (i = 0; *file->c != '\0'; i++) {
		if (*file->c == '\"') {
			seen_delimiter = true;
			file->c++;
			break;
		}

		if (*file->c == '\\') {
			file->c++;
			strbuf_putc(&file->strbuf, read_escape_sequence(file));
		}
		else {
			strbuf_putc(&file->strbuf, *file->c);
			file->c++;
		}
	}

	if (!seen_delimiter) {
		DEBUG_MSG("error: missing the final \"");
	}

	tokinfo->token = TOKEN_STRING;
	tokinfo->str = strbuf_copy_to_mempool(&file->strbuf, &file->token_data);

	if (!tokinfo->str)
		return NULL;

	return tokinfo;
}


static bool is_valid_qchar(char c)
{
	switch (c) {
	case '\n':
	case '\r':
	case '\'':
	case '\\':
	case '\"':
		return false;

	default:
		return true;
	}
}


static bool is_valid_hchar(char c)
{
	return is_valid_qchar(c) || c != '\"';
}


struct tokinfo *lex_header_name(struct cppfile *file, struct tokinfo *tokinfo,
	bool (*is_valid_char)(char c), char delim)
{
	char c;
	bool seen_delim = false;

	strbuf_reset(&file->strbuf);

	while ((c = *file->c++) != '\0') {
		if (c == delim) {
			seen_delim = true;
			break;
		}

		if (!is_valid_char(c)) {
			DEBUG_PRINTF("error: unexpected char: %c", c);
		}
		else {
			strbuf_putc(&file->strbuf, c);
		}
	}

	if (!seen_delim)
		DEBUG_PRINTF("error: %c was expected here", delim);

	// TODO Check (and warn about) presence of // and /* comments within header name

	tokinfo->token = TOKEN_HEADER_NAME;
	tokinfo->str = strbuf_copy_to_mempool(&file->strbuf, &file->token_data);

	if (!tokinfo->str)
		return NULL;

	return tokinfo;
}


struct tokinfo *lex_header_hname(struct cppfile *file, struct tokinfo *tokinfo)
{
	return lex_header_name(file, tokinfo, is_valid_hchar, '>');
}


struct tokinfo *lex_header_qname(struct cppfile *file, struct tokinfo *tokinfo)
{
	return lex_header_name(file, tokinfo, is_valid_qchar, '\"');
}


/*
 * TODO Add trigraph support.
 */
static mcc_error_t lexer_read_line(struct cppfile *file)
{
	int c;
	bool escape = false;

	strbuf_reset(&file->linebuf);

	for (; (c = inbuf_get_char(&file->inbuf)) != INBUF_EOF; ) {
		switch (c) {
		case ' ':
		case '\t':
		case '\v':
		case '\f':
		// TODO more cases
			strbuf_putc(&file->linebuf, c);
			continue;

		case '\n':
			if (escape) {
				escape = false;
				continue;
			}

			goto eol_or_eof;

		case '\\':
			if (!escape) {
				escape = true;
				break;
			}

		default:
			if (escape) {
				strbuf_putc(&file->linebuf, '\\');
			}

			strbuf_putc(&file->linebuf, c);
			escape = false;
		}
	}

	if (c == INBUF_EOF)
		return MCC_ERROR_EOF;

eol_or_eof:
	file->c = strbuf_get_string(&file->linebuf);

	return MCC_ERROR_OK;
}


static inline void eat_whitespace(struct cppfile *file)
{
	while (is_whitespace(*file->c))
		file->c++;
}


void eat_cpp_comment(struct cppfile *file)
{
	lexer_read_line(file);
}


void eat_c_comment(struct cppfile *file)
{
	assert(file->c[-2] == '/' && file->c[-1] == '*');

search_comment_terminator:
	while (!lexer_is_eol(file)) {
		if (*file->c == '/' && file->c[-1] == '*') {
			file->c += 2;
			return;
		}

		file->c++;
	}

	if (lexer_read_line(file))
		goto search_comment_terminator;

	DEBUG_MSG("error: missing */");
}


struct tokinfo *lexer_next(struct cppfile *file)
{
	struct tokinfo *tokinfo;
	mcc_error_t err;

next_nonwhite_char:
	if (lexer_is_eol(file)) {
		err = lexer_read_line(file);
		file->next_at_bol = true;

		if (err == MCC_ERROR_EOF)
			return &eof;

		if (err != MCC_ERROR_OK)
			return NULL;

		if (!file->first_token)
			return &eol;
	}

	file->first_token = false;

	eat_whitespace(file);

	tokinfo = objpool_alloc(&file->tokinfo_pool);
	if (!tokinfo)
		return NULL;

	tokinfo->is_at_bol = file->next_at_bol;
	file->next_at_bol = false;

	file->c++;
	switch (file->c[-1]) {
	case 'u':
		/* u8"string" */
		if (*file->c == '8' && file->c[1] == '\"') {
			file->c += 2;
			return lex_string(file, tokinfo);
		}

	case 'U':
	case 'L':
		/* L'char' or U'char' or u'char' */
		if (*file->c == '\'') {
			file->c++;
			return lex_char(file, tokinfo);
		}
		
		/* L"string" or U"string" or u"string" */
		if (*file->c == '\"') {
			file->c++;
			return lex_string(file, tokinfo);
		}

	case '_': case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm':
	case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
	case 'v': case 'w': case 'x': case 'y': case 'z': case 'A': case 'B':
	case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I':
	case 'J': case 'K': case 'M': case 'N': case 'O': case 'P': case 'Q':
	case 'R': case 'S': case 'T': case 'V': case 'W': case 'X': case 'Y':
	case 'Z': case '\\':
		file->c--;
		return lex_name(file, tokinfo);

	case '\"':
		if (file->inside_include)
			return lex_header_qname(file, tokinfo);
		else
			return lex_string(file, tokinfo);

	case '\'':
		return lex_char(file, tokinfo);

	case '.':
		if (!is_digit(*file->c)) {
			if (*file->c == '.' && file->c[1] == '.') {
				tokinfo->token = TOKEN_ELLIPSIS;
				file->c += 2;
			}
			else {
				tokinfo->token = TOKEN_DOT;
			}

			break;
		}

		/* fall-through to pp-number processing */ 

	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9':
		file->c--;
		return lex_pp_number(file, tokinfo);

	case '[':
		tokinfo->token = TOKEN_LBRACKET;
		break;

	case ']':
		tokinfo->token = TOKEN_RBRACKET;
		break;

	case '(':
		tokinfo->token = TOKEN_LPAREN;
		break;

	case ')':
		tokinfo->token = TOKEN_RPAREN;
		break;

	case '{':
		tokinfo->token = TOKEN_LBRACE;
		break;

	case '}':
		tokinfo->token = TOKEN_RBRACE;
		break;

	case '-':
		if (*file->c == '>') {
			tokinfo->token = TOKEN_ARROW;
			file->c++;
		}
		else if (*file->c == '-') {
			tokinfo->token = TOKEN_DEC;
			file->c++;
		}
		else if (*file->c == '=') {
			tokinfo->token = TOKEN_MINUS_EQ;
			file->c++;
		}
		else {
			tokinfo->token = TOKEN_MINUS;
		}

		break;

	case '+':
		if (*file->c == '+') {
			tokinfo->token = TOKEN_INC;
			file->c++;
		}
		else if (*file->c == '=') {
			tokinfo->token = TOKEN_PLUS_EQ;
			file->c++;
		}
		else {
			tokinfo->token = TOKEN_PLUS;
		}

		break;

	case '&':
		if (*file->c == '=') {
			tokinfo->token = TOKEN_AND_EQ;
			file->c++;
		}
		else if (*file->c == '&') {
			tokinfo->token = TOKEN_LOGICAL_AND;
			file->c++;
		}
		else {
			tokinfo->token = TOKEN_AMPERSAND;
		}

		break;

	case '*':
		if (*file->c == '=') {
			tokinfo->token = TOKEN_MUL_EQ;
			file->c++;
		}
		else {
			tokinfo->token = TOKEN_ASTERISK;
		}

		break;

	case '~':
		tokinfo->token = TOKEN_NEQ;
		break;

	case '!':
		if (*file->c == '=') {
			tokinfo->token = TOKEN_NEQ;
			file->c++;
		}
		else {
			tokinfo->token = TOKEN_NOT;
		}

		break;

	case '/':
		if (*file->c == '*') {
			file->c++;
			eat_c_comment(file);
			goto next_nonwhite_char;
		}

		if (*file->c == '/') {
			file->c++;
			eat_cpp_comment(file);
			goto next_nonwhite_char;
		}

		if (*file->c == '=') {
			tokinfo->token = TOKEN_DIV_EQ;
			file->c++;
		}
		else {
			tokinfo->token = TOKEN_DIV;
		}

		break;

	case '%':
		if (*file->c == '=') {
			tokinfo->token = TOKEN_MOD_EQ;
			file->c++;
		}
		else if (*file->c == ':') {
			if (file->c[1] == '%' && file->c[2] == ':') {
				tokinfo->token = TOKEN_HASH_HASH;
				file->c += 3;
			}
			else {
				tokinfo->token = TOKEN_HASH;
				file->c++;
			}
		}

	case '<':
		if (!file->inside_include) {
			if (*file->c == '=') {
				tokinfo->token = TOKEN_LE;
			}
			else if (*file->c == '<') {
				if (file->c[1] == '=') {
					tokinfo->token = TOKEN_SHR_EQ;
					file->c += 2;
				}
				else {
					tokinfo->token = TOKEN_SHR;
					file->c++;
				}
			}
			else if (file->c[1] == '%') {
				tokinfo->token = TOKEN_LBRACE;
				file->c++;
			}
			else if (file->c[1] == ':') {
				tokinfo->token = TOKEN_LBRACKET;
				file->c++;
			}
			else {
				tokinfo->token = TOKEN_LT;
			}
			break;
		}
		else {
			return lex_header_hname(file, tokinfo);
		}

	case '>':
		if (*file->c == '=') {
			tokinfo->token = TOKEN_GE;
			file->c++;
		}
		else if (*file->c == '>') {
			if (file->c[1] == '=') {
				tokinfo->token = TOKEN_SHR_EQ;
				file->c += 2;
			}
			else {
				tokinfo->token = TOKEN_SHR;
				file->c++;
			}
		}
		else {
			tokinfo->token = TOKEN_GT;
		}

		break;

	case '^':
		if (*file->c == '=') {
			tokinfo->token = TOKEN_XOR_EQ;
			file->c++;
		}
		else {
			tokinfo->token = TOKEN_EQ;
		}

		break;

	case '|':
		if (*file->c == '|') {
			tokinfo->token = TOKEN_LOGICAL_OR;
			file->c++;
		}
		else if (*file->c == '=') {
			tokinfo->token = TOKEN_OR_EQ;
			file->c++;
		}
		else {
			tokinfo->token = TOKEN_OR;
		}

		break;
	
	case '?':
		tokinfo->token = TOKEN_QUESTION_MARK;
		break;

	case ':':
		if (*file->c == '>') {
			tokinfo->token = TOKEN_RBRACKET;
			file->c++;
		}
		else {
			tokinfo->token = TOKEN_COLON;
		}

		break;

	case ';':
		tokinfo->token = TOKEN_SEMICOLON;
		break;

	case '=':
		if (*file->c == '=') {
			tokinfo->token = TOKEN_EQ_EQ;
			file->c++;
		}
		else {
			tokinfo->token = TOKEN_EQ;
		}

		break;

	case ',':
		tokinfo->token = TOKEN_COMMA;
		break;

	case '#':
		if (file->c[1] == '#') {
			tokinfo->token = TOKEN_HASH_HASH;
			file->c += 2;
		}
		else {
			tokinfo->token = TOKEN_HASH;
		}

		break;

	default:
		DEBUG_PRINTF("error: character %c was unexpected", *file->c);
		file->c++;
		goto next_nonwhite_char;
	}

	return tokinfo;
}
