#include "debug.h"
#include "lexer.h"
#include "tokinfo.h"
#include "symtab.h"
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>


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


static int32_t read_octal_number(struct lexer *lexer)
{
	uint32_t val = 0;
	int i;

	for (i = 0; i < 3; i++) {
		if (!is_octal_digit(*lexer->c))
			break;

		val *= 8;
		val += *lexer->c - '0';

		lexer->c++;
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


static uint32_t read_hex_number(struct lexer *lexer, size_t min_len, size_t max_len)
{	
	uint32_t val = 0;
	int i;

	for (i = 0; i < max_len; i++) {
		if (!is_hex_digit(*lexer->c))
			break;

		val *= 16;
		val += hex_val(*lexer->c);

		lexer->c++;
	}

	if (i < min_len)
		DEBUG_MSG("error: hex digit expected");


	return val;
}


static uint32_t read_escape_sequence(struct lexer *lexer)
{
	assert(lexer->c[-1] == '\\');

	int c = *lexer->c;
	lexer->c++;

	if (simple_escape_seq[c])
		return simple_escape_seq[c];

	switch (c) {
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
		lexer->c--;
		return read_octal_number(lexer);

	case 'x':	
		return read_hex_number(lexer, 1, 8);

	case 'u':
		return read_hex_number(lexer, 4, 4);

	case 'U':
		return read_hex_number(lexer, 8, 8);

	default:
		DEBUG_PRINTF("error: unknown escape sequence: \\%c", *lexer->c);
		lexer->c++;
		return -1;
	}
}


struct symbol *lexer_upsert_symbol(struct lexer *lexer, char *name)
{
	struct symbol *symbol;
	
	symbol = symtab_search(lexer->symtab, name);
	if (!symbol) {
		symbol = symtab_insert(lexer->symtab, name);
		symbol->type = SYMBOL_TYPE_UNKNOWN;
	}

	return symbol;
}


mcc_error_t lex_name(struct lexer *lexer, struct tokinfo *tokinfo)
{
	strbuf_reset(&lexer->strbuf);

	while (is_letter(*lexer->c) || is_digit(*lexer->c) || *lexer->c == '_' || *lexer->c == '\\') {
		strbuf_putc(&lexer->strbuf, *lexer->c); // TODO UCN support affects this
		lexer->c++;
	}

	tokinfo->token = TOKEN_NAME;
	tokinfo->symbol = lexer_upsert_symbol(lexer, strbuf_get_string(&lexer->strbuf));

	if (tokinfo->symbol != NULL)
		return MCC_ERROR_OK;

	return MCC_ERROR_NOMEM;
}


static mcc_error_t lex_char(struct lexer *lexer, struct tokinfo *tokinfo)
{
	uint32_t val;
	bool seen_delimiter = false;
	size_t i;

	assert(lexer->c[-1] == '\'');

	for (i = 0; *lexer->c != '\0'; i++) {
		if (*lexer->c == '\'') {
			seen_delimiter = true;
			lexer->c++;
			break;
		}

		if (*lexer->c == '\\') {
			lexer->c++;
			val = read_escape_sequence(lexer);
		}
		else {
			val = (int)(*lexer->c);
			lexer->c++;
		}
	}

	if (i == 0)
		DEBUG_MSG("error: empty character constant");

	if (i > 1)
		DEBUG_MSG("error: multichar character constants are not supported")

	if (!seen_delimiter)
		DEBUG_MSG("error: missing the final \'");

	tokinfo->value = val;
	tokinfo->token = TOKEN_CHAR_CONST;

	return MCC_ERROR_OK;
}


static inline bool lexer_is_eol(struct lexer *lexer)
{
	return (lexer->c - strbuf_get_string(&lexer->linebuf))
		>= strbuf_strlen(&lexer->linebuf);
}


mcc_error_t lex_pp_number(struct lexer *lexer, struct tokinfo *tokinfo)
{
	char c;

	strbuf_reset(&lexer->strbuf);

	while (!lexer_is_eol(lexer)) {
		if (*lexer->c == '+' || *lexer->c == '-') {
			/* assert(not at the BOL) */
			assert(lexer->c != strbuf_get_string(&lexer->linebuf));

			c = lexer->c[-1];
			if (c != 'e' && c != 'E' && c != 'p' && c != 'P') {
				break;
			}
		}
		else if (*lexer->c == '\\') {
			// process UCN
			lexer->c++;
			continue;
		}
		else if (!is_digit(*lexer->c) && !is_letter(*lexer->c) && *lexer->c != '.' && *lexer->c != '_') {
			break;
		}

		strbuf_putc(&lexer->strbuf, *lexer->c);
		lexer->c++;
	}

	tokinfo->token = TOKEN_NUMBER;
	tokinfo->str = strbuf_copy_to_mempool(&lexer->strbuf, &lexer->token_data);

	if (tokinfo->str)
		return MCC_ERROR_OK;

	return MCC_ERROR_NOMEM;
}


mcc_error_t lex_string(struct lexer *lexer, struct tokinfo *tokinfo)
{
	strbuf_reset(&lexer->strbuf);

	bool seen_delimiter = false;
	size_t i;

	assert(lexer->c[-1] == '\"');

	for (i = 0; *lexer->c != '\0'; i++) {
		if (*lexer->c == '\"') {
			seen_delimiter = true;
			lexer->c++;
			break;
		}

		if (*lexer->c == '\\') {
			lexer->c++;
			strbuf_putc(&lexer->strbuf, read_escape_sequence(lexer));
		}
		else {
			strbuf_putc(&lexer->strbuf, *lexer->c);
			lexer->c++;
		}
	}

	if (!seen_delimiter) {
		DEBUG_MSG("error: missing the final \"");
	}

	tokinfo->token = TOKEN_STRING;
	tokinfo->str = strbuf_copy_to_mempool(&lexer->strbuf, &lexer->token_data);

	if (tokinfo->str)
		return MCC_ERROR_OK;

	return MCC_ERROR_NOMEM;
}


static bool is_valid_qchar(struct lexer *lexer)
{
	if (*lexer->c == '\n' || *lexer->c == '\r')
		return false;

	if (*lexer->c == '\'' || *lexer->c == '\\' || *lexer->c == '\"')
		return false;

	if (*lexer->c == '/' && (lexer->c[1] == '/' || lexer->c[1] == '*'))
		return false;

	return true;
}


static bool is_valid_hchar(struct lexer *lexer)
{
	return is_valid_qchar(lexer) || *lexer->c != '\"';
}


mcc_error_t lex_header_hname(struct lexer *lexer, struct tokinfo *tokinfo)
{
	strbuf_reset(&lexer->strbuf);

	while (*lexer->c != '>') {
		if (!is_valid_hchar(lexer)) {
			DEBUG_PRINTF("error: invalid hchar: %c", *lexer->c);
		}
		else {
			strbuf_putc(&lexer->strbuf, *lexer->c);
		}

		lexer->c++;
	}

	tokinfo->token = TOKEN_HEADER_NAME;
	tokinfo->str = strbuf_copy_to_mempool(&lexer->strbuf, &lexer->token_data);

	return MCC_ERROR_OK;
}


mcc_error_t lex_header_qname(struct lexer *lexer, struct tokinfo *tokinfo)
{
	strbuf_reset(&lexer->strbuf);

	while (*lexer->c != '\"') {
		if (!is_valid_qchar(lexer)) {
			DEBUG_PRINTF("error: invalid qchar: %c", *lexer->c);
		}
		else {
			strbuf_putc(&lexer->strbuf, *lexer->c);
		}

		lexer->c++;
	}

	tokinfo->token = TOKEN_HEADER_NAME;
	tokinfo->str = strbuf_copy_to_mempool(&lexer->strbuf, &lexer->token_data);

	return MCC_ERROR_OK;
}


mcc_error_t lexer_init(struct lexer *lexer)
{
	lexer->inside_include = false;
	lexer->inbuf = NULL;

	if (!strbuf_init(&lexer->linebuf, 1024))
		return MCC_ERROR_NOMEM;

	if (!strbuf_init(&lexer->strbuf, 1024))
		return MCC_ERROR_NOMEM;

	lexer->c = strbuf_get_string(&lexer->linebuf);

	objpool_init(&lexer->tokinfo_pool, sizeof(struct tokinfo), 64);
	mempool_init(&lexer->token_data, 1024);

	return MCC_ERROR_OK;
}


void lexer_set_inbuf(struct lexer *lexer, struct inbuf *inbuf)
{
	assert(lexer->inbuf == NULL); // TODO
	lexer->inbuf = inbuf;
}


void lexer_free(struct lexer *lexer)
{
	strbuf_free(&lexer->linebuf);
	strbuf_free(&lexer->strbuf);
	mempool_free(&lexer->token_data);
	objpool_free(&lexer->tokinfo_pool);
}


/*
 * TODO Add trigraph support.
 */
static mcc_error_t lexer_read_line(struct lexer *lexer)
{
	assert(lexer->inbuf != NULL);

	int c;
	bool escape = false;

	strbuf_reset(&lexer->linebuf);

	for (; (c = inbuf_get_char(lexer->inbuf)) != INBUF_EOF; ) {
		switch (c) {
		case ' ':
		case '\t':
		case '\v':
		case '\f':
		// TODO more cases
			strbuf_putc(&lexer->linebuf, c);
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
				strbuf_putc(&lexer->linebuf, '\\');
			}

			strbuf_putc(&lexer->linebuf, c);
			escape = false;
		}
	}

	if (c == INBUF_EOF)
		return MCC_ERROR_EOF;

eol_or_eof:
	lexer->c = strbuf_get_string(&lexer->linebuf);

	return MCC_ERROR_OK;
}


static inline void eat_whitespace(struct lexer *lexer)
{
	while (is_whitespace(*lexer->c))
		lexer->c++;
}


void eat_cpp_comment(struct lexer *lexer)
{
	lexer_read_line(lexer);
}


void eat_c_comment(struct lexer *lexer)
{
	assert(lexer->c[-2] == '/' && lexer->c[-1] == '*');

search_comment_terminator:
	while (!lexer_is_eol(lexer)) {
		if (*lexer->c == '/' && lexer->c[-1] == '*') {
			lexer->c += 2;
			return;
		}

		lexer->c++;
	}

	if (lexer_read_line(lexer))
		goto search_comment_terminator;

	DEBUG_MSG("error: missing */");
}


mcc_error_t lexer_next(struct lexer *lexer, struct tokinfo *tokinfo)
{
	mcc_error_t err;

	tokinfo->is_at_bol = false;

	//tokinfo = objpool_alloc(&lexer->tokinfo_pool);

smash_whitespace:
	if (lexer_is_eol(lexer)) {
		err = lexer_read_line(lexer);
		tokinfo->is_at_bol = true;

		if (err == MCC_ERROR_OK)
			goto smash_whitespace;
		else
			return err;
	}
	else {
		eat_whitespace(lexer);
	}

	lexer->c++;

	switch (lexer->c[-1]) {
	case 'u':
		/* u8"string" */
		if (*lexer->c == '8' && lexer->c[1] == '\"') {
			lexer->c += 2;
			return lex_string(lexer, tokinfo);
		}

	case 'U':
	case 'L':
		/* L'char' or U'char' or u'char' */
		if (*lexer->c == '\'') {
			lexer->c++;
			return lex_char(lexer, tokinfo);
		}
		
		/* L"string" or U"string" or u"string" */
		if (*lexer->c == '\"') {
			lexer->c++;
			return lex_string(lexer, tokinfo);
		}

	case '_': case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm':
	case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
	case 'v': case 'w': case 'x': case 'y': case 'z': case 'A': case 'B':
	case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I':
	case 'J': case 'K': case 'M': case 'N': case 'O': case 'P': case 'Q':
	case 'R': case 'S': case 'T': case 'V': case 'W': case 'X': case 'Y':
	case 'Z': case '\\':
		lexer->c--;
		return lex_name(lexer, tokinfo);

	case '\"':
		if (lexer->inside_include)
			return lex_header_hname(lexer, tokinfo);
		else
			return lex_string(lexer, tokinfo);

	case '\'':
		return lex_char(lexer, tokinfo);

	case '.':
		if (!is_digit(*lexer->c)) {
			if (*lexer->c == '.' && lexer->c[1] == '.') {
				tokinfo->token = TOKEN_ELLIPSIS;
				lexer->c += 2;
			}
			else {
				tokinfo->token = TOKEN_DOT;
			}

			break;
		}

		/* fall-through to pp-number processing */ 

	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9':
		lexer->c--;
		return lex_pp_number(lexer, tokinfo);

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
		if (*lexer->c == '>') {
			tokinfo->token = TOKEN_ARROW;
			lexer->c++;
		}
		else if (*lexer->c == '-') {
			tokinfo->token = TOKEN_DEC;
			lexer->c++;
		}
		else if (*lexer->c == '=') {
			tokinfo->token = TOKEN_MINUS_EQ;
			lexer->c++;
		}
		else {
			tokinfo->token = TOKEN_MINUS;
		}

		break;

	case '+':
		if (*lexer->c == '+') {
			tokinfo->token = TOKEN_INC;
			lexer->c++;
		}
		else if (*lexer->c == '=') {
			tokinfo->token = TOKEN_PLUS_EQ;
			lexer->c++;
		}
		else {
			tokinfo->token = TOKEN_PLUS;
		}

		break;

	case '&':
		if (*lexer->c == '=') {
			tokinfo->token = TOKEN_AND_EQ;
			lexer->c++;
		}
		else if (*lexer->c == '&') {
			tokinfo->token = TOKEN_LOGICAL_AND;
			lexer->c++;
		}
		else {
			tokinfo->token = TOKEN_AMPERSAND;
		}

		break;

	case '*':
		if (*lexer->c == '=') {
			tokinfo->token = TOKEN_MUL_EQ;
			lexer->c++;
		}
		else {
			tokinfo->token = TOKEN_ASTERISK;
		}

		break;

	case '~':
		tokinfo->token = TOKEN_NEQ;
		break;

	case '!':
		if (*lexer->c == '=') {
			tokinfo->token = TOKEN_NEQ;
			lexer->c++;
		}
		else {
			tokinfo->token = TOKEN_NOT;
		}

		break;

	case '/':
		if (*lexer->c == '*') {
			lexer->c++;
			eat_c_comment(lexer);
			goto smash_whitespace;
		}

		if (*lexer->c == '/') {
			lexer->c++;
			eat_cpp_comment(lexer);
			goto smash_whitespace;
		}

		if (*lexer->c == '=') {
			tokinfo->token = TOKEN_DIV_EQ;
			lexer->c++;
		}
		else {
			tokinfo->token = TOKEN_DIV;
		}

		break;

	case '%':
		if (*lexer->c == '=') {
			tokinfo->token = TOKEN_MOD_EQ;
			lexer->c++;
		}
		else if (*lexer->c == ':') {
			if (lexer->c[1] == '%' && lexer->c[2] == ':') {
				tokinfo->token = TOKEN_HASH_HASH;
				lexer->c += 3;
			}
			else {
				tokinfo->token = TOKEN_HASH;
				lexer->c++;
			}
		}

	case '<':
		if (!lexer->inside_include) {
			if (*lexer->c == '=') {
				tokinfo->token = TOKEN_LE;
			}
			else if (*lexer->c == '<') {
				if (lexer->c[1] == '=') {
					tokinfo->token = TOKEN_SHR_EQ;
					lexer->c += 2;
				}
				else {
					tokinfo->token = TOKEN_SHR;
					lexer->c++;
				}
			}
			else if (lexer->c[1] == '%') {
				tokinfo->token = TOKEN_LBRACE;
				lexer->c++;
			}
			else if (lexer->c[1] == ':') {
				tokinfo->token = TOKEN_LBRACKET;
				lexer->c++;
			}
			else {
				tokinfo->token = TOKEN_LT;
			}
			break;
		}
		else {
			return lex_header_hname(lexer, tokinfo);
		}

	case '>':
		if (*lexer->c == '=') {
			tokinfo->token = TOKEN_GE;
			lexer->c++;
		}
		else if (*lexer->c == '>') {
			if (lexer->c[1] == '=') {
				tokinfo->token = TOKEN_SHR_EQ;
				lexer->c += 2;
			}
			else {
				tokinfo->token = TOKEN_SHR;
				lexer->c++;
			}
		}
		else {
			tokinfo->token = TOKEN_GT;
		}

		break;

	case '^':
		if (*lexer->c == '=') {
			tokinfo->token = TOKEN_XOR_EQ;
			lexer->c++;
		}
		else {
			tokinfo->token = TOKEN_EQ;
		}

		break;

	case '|':
		if (*lexer->c == '|') {
			tokinfo->token = TOKEN_LOGICAL_OR;
			lexer->c++;
		}
		else if (*lexer->c == '=') {
			tokinfo->token = TOKEN_OR_EQ;
			lexer->c++;
		}
		else {
			tokinfo->token = TOKEN_OR;
		}

		break;
	
	case '?':
		tokinfo->token = TOKEN_QUESTION_MARK;
		break;

	case ':':
		if (*lexer->c == '>') {
			tokinfo->token = TOKEN_RBRACKET;
			lexer->c++;
		}
		else {
			tokinfo->token = TOKEN_COLON;
		}

		break;

	case ';':
		tokinfo->token = TOKEN_SEMICOLON;
		break;

	case '=':
		if (*lexer->c == '=') {
			tokinfo->token = TOKEN_EQ_EQ;
			lexer->c++;
		}
		else {
			tokinfo->token = TOKEN_EQ;
		}

		break;

	case ',':
		tokinfo->token = TOKEN_COMMA;
		break;

	case '#':
		if (lexer->c[1] == '#') {
			tokinfo->token = TOKEN_HASH_HASH;
			lexer->c += 2;
		}
		else {
			tokinfo->token = TOKEN_HASH;
		}

		break;

	default:
		DEBUG_PRINTF("error: character %c was unexpected", *lexer->c);
		lexer->c++;
	}

	return MCC_ERROR_OK;
}


size_t char_to_printable(char c, char *str, size_t len)
{
	switch (c)
	{
	case '\a':
		return snprintf(str, len, "\\a");

	case '\b':
		return snprintf(str, len, "\\b");

	case '\f':
		return snprintf(str, len, "\\f");

	case '\r':
		return snprintf(str, len, "\\r");

	case '\n':
		return snprintf(str, len, "\\n");

	case '\v':
		return snprintf(str, len, "\\v");

	case '\t':
		return snprintf(str, len, "\\t");

	case '\\':
		return snprintf(str, len, "\\");

	case '\"':
		return snprintf(str, len, "\\\"");

	case '\'':
		return snprintf(str, len, "\\\'");

	case '\?':
		return snprintf(str, len, "\\\?");
	}

	if (isprint(c))
		return snprintf(str, len, "%c", c);
	else
		return snprintf(str, len, "0x%x", c);
}


void lexer_dump_token(struct tokinfo *tokinfo)
{
	const char *name = token_name(tokinfo->token);
	char tmp[32];
	char c;
	size_t i = 0;

	switch (tokinfo->token) {
	case TOKEN_STRING:
		printf("\"");
		while ((c = tokinfo->str[i++])) {
			char_to_printable(c, tmp, sizeof(tmp));
			printf("%s", tmp);
		}
		printf("\"");
		break;

	case TOKEN_CHAR_CONST:
		char_to_printable(tokinfo->value, tmp, sizeof(tmp));
		printf("\'%s\'", tmp);
		break;

	case TOKEN_NAME:
		printf("%s[%s]", symbol_get_name(tokinfo->symbol), symbol_get_type(tokinfo->symbol));
		break;

	case TOKEN_NUMBER:
		printf("%s", tokinfo->str);
		break;

	case TOKEN_HEADER_NAME:
		printf("HEADER_NAME\"%s\"", tokinfo->str);
		break;

	default:
		printf("%s", name);
	}

	putchar(' ');
}


void lexer_set_symtab(struct lexer *lexer, struct symtab *symtab)
{
	lexer->symtab = symtab;
}
