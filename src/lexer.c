#include "debug.h"
#include "lexer.h"
#include "print.h"
#include "symbol.h"
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define INBUF_BLOCK_SIZE	2048
#define STRBUF_INIT_SIZE	128

static struct token eof = { .type = TOKEN_EOF };

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


mcc_error_t lexer_init(struct lexer *lexer, struct cpp *cpp, char *filename)
{
	mcc_error_t err;

	if (!strbuf_init(&lexer->linebuf, STRBUF_INIT_SIZE))
		return MCC_ERROR_NOMEM;

	if (!strbuf_init(&lexer->strbuf, STRBUF_INIT_SIZE)) {
		strbuf_free(&lexer->linebuf);
		return MCC_ERROR_NOMEM;
	}

	if (!strbuf_init(&lexer->spelling, STRBUF_INIT_SIZE)) {
		strbuf_free(&lexer->linebuf);
		strbuf_free(&lexer->strbuf);
		return MCC_ERROR_NOMEM;
	}

	err = inbuf_open(&lexer->inbuf, INBUF_BLOCK_SIZE, filename);
	if (err != MCC_ERROR_OK)
		goto out_err;

	lexer->c = strbuf_get_string(&lexer->linebuf);

	lexer->cpp = cpp;
	lexer->filename = filename;
	lexer->location.line_no = 0;
	lexer->inside_include = false;
	lexer->emit_eols = false;
	lexer->next_at_bol = true;
	lexer->first_token = true;
	lexer->had_whitespace = false;

	return MCC_ERROR_OK;

out_err:
	strbuf_free(&lexer->linebuf);
	strbuf_free(&lexer->strbuf);
	strbuf_free(&lexer->spelling);

	return err;
}


void lexer_free(struct lexer *lexer)
{
	strbuf_free(&lexer->linebuf);
	strbuf_free(&lexer->strbuf);
	strbuf_free(&lexer->spelling);
	inbuf_close(&lexer->inbuf);
}


static void lexer_error_internal(struct lexer *lexer, enum error_level level,
	char *fmt, va_list args, char *context, struct location location)
{
	struct strbuf msg;

	strbuf_init(&msg, 128);
	strbuf_vprintf_at(&msg, 0, fmt, args);

	errlist_insert(&lexer->cpp->errlist,
		level,
		lexer->filename,
		strbuf_get_string(&msg),
		context,
		lexer->location);

	strbuf_free(&msg);
}


static void lexer_error(struct lexer *lexer, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	lexer_error_internal(lexer, ERROR_LEVEL_ERROR, fmt, args,
		strbuf_get_string(&lexer->linebuf), lexer->location);
	va_end(args);
}


static void lexer_error_noctx(struct lexer *lexer, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	lexer_error_internal(lexer, ERROR_LEVEL_ERROR, fmt, args, NULL,
		lexer->location);
	va_end(args);
}


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
	return c == '\t' || c == 0x0C || c == ' ';
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
		lexer_error(lexer, "octal digit expected");
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
		lexer_error(lexer, "hex digit expected");


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
		lexer_error(lexer, "unknown escape sequence: \\%c", *lexer->c);
		lexer->c++;
		return -1;
	}
}


struct token *lexer_lex_name(struct lexer *lexer, struct token *token)
{
	strbuf_reset(&lexer->strbuf);

	while (is_letter(*lexer->c)
		|| is_digit(*lexer->c)
		|| *lexer->c == '_'
		|| *lexer->c == '\\') {
		strbuf_putc(&lexer->strbuf, *lexer->c); // TODO UCN support affects this
		lexer->c++;
	}

	token->type = TOKEN_NAME;
	token->symbol = symtab_search_or_insert(lexer->cpp->symtab, strbuf_get_string(&lexer->strbuf));

	if (!token->symbol)
		return NULL;

	return token;
}


static struct token *lexer_lex_char(struct lexer *lexer, struct token *token)
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
		lexer_error(lexer, "empty character constant");

	if (i > 1)
		lexer_error(lexer, "multichar character constants are not supported");

	if (!seen_delimiter)
		lexer_error(lexer, "missing the final \'");

	token->type = TOKEN_CHAR_CONST;
	token->value = val;

	return token;
}


static inline bool lexer_is_eol(struct lexer *lexer)
{
	return (lexer->c - strbuf_get_string(&lexer->linebuf))
		>= strbuf_strlen(&lexer->linebuf);
}


struct token *lexer_lex_pp_number(struct lexer *lexer, struct token *token)
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

	token->type = TOKEN_NUMBER;
	token->str = strbuf_copy_to_mempool(&lexer->strbuf, &lexer->cpp->token_data);

	if (!token->str)
		return NULL;

	return token;
}


char *lexer_spell(struct lexer *lexer, char *start, char *end)
{
	char *c;

	strbuf_reset(&lexer->spelling);
	for (c = start; c != end; c++)
		strbuf_putc(&lexer->spelling, *c);

	return strbuf_copy_to_mempool(&lexer->spelling, &lexer->cpp->token_data);
}


struct token *lexer_lex_string(struct lexer *lexer, struct token *token)
{
	bool seen_delimiter = false;
	size_t i;
	char *start;

	strbuf_reset(&lexer->strbuf);
	assert(lexer->c[-1] == '\"');

	start = lexer->c - 1;

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
		lexer_error(lexer, "missing the final \"");
	}

	token->type = TOKEN_STRING;
	token->str = strbuf_copy_to_mempool(&lexer->strbuf, &lexer->cpp->token_data);
	token->spelling = lexer_spell(lexer, start, lexer->c);

	if (!token->str)
		return NULL;

	return token;
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


struct token *lexer_lex_header_name(struct lexer *lexer, struct token *token,
	bool (*is_valid_char)(char c), char delim)
{
	char c;
	bool seen_delim = false;

	strbuf_reset(&lexer->strbuf);

	while ((c = *lexer->c++) != '\0') {
		if (c == delim) {
			seen_delim = true;
			break;
		}

		if (!is_valid_char(c)) {
			lexer_error(lexer, "unexpected char: %c", c);
		}
		else {
			strbuf_putc(&lexer->strbuf, c);
		}
	}

	if (!seen_delim)
		lexer_error(lexer, "%c was expected here", delim);

	// TODO Check (and warn about) presence of // and /* comments within header name

	token->str = strbuf_copy_to_mempool(&lexer->strbuf, &lexer->cpp->token_data);

	if (!token->str)
		return NULL;

	return token;
}


struct token *lexer_lex_header_hname(struct lexer *lexer, struct token *token)
{
	token->type = TOKEN_HEADER_HNAME;
	lexer_lex_header_name(lexer, token, is_valid_hchar, '>');
	return token;
}


struct token *lexer_lex_header_qname(struct lexer *lexer, struct token *token)
{
	token->type = TOKEN_HEADER_QNAME;
	lexer_lex_header_name(lexer, token, is_valid_qchar, '\"');
	return token;
}


/*
 * TODO Add trigraph support.
 */
static mcc_error_t lexer_read_line(struct lexer *lexer)
{
	int c;
	bool escape = false;

	strbuf_reset(&lexer->linebuf);

	for (lexer->location.column_no = 0; (c = inbuf_get_char(&lexer->inbuf)) != INBUF_EOF; lexer->location.column_no++) {
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
	lexer->location.line_no++;

	return MCC_ERROR_OK;
}


static inline void eat_whitespace(struct lexer *lexer)
{
	while (is_whitespace(*lexer->c)) {
		lexer->had_whitespace = true;
		lexer->c++;
	}
}


void eat_cpp_comment(struct lexer *lexer)
{
	while (!lexer_is_eol(lexer)) {
		lexer->c++;
	}
}


void eat_c_comment(struct lexer *lexer)
{
	assert(lexer->c[-2] == '/' && lexer->c[-1] == '*');

search_comment_terminator:
	while (!lexer_is_eol(lexer)) {
		/* TODO Wrong, reading invalid memory in some cases */
		if (*lexer->c == '/' && lexer->c[-1] == '*') {
			lexer->c += 2;
			return;
		}

		lexer->c++;
	}

	if (lexer_read_line(lexer) != MCC_ERROR_EOF)
		goto search_comment_terminator;

	lexer_error_noctx(lexer, "missing */");
}


static struct token *new_eol(struct lexer *lexer)
{
	struct token *token;

	token = objpool_alloc(&lexer->cpp->token_pool);
	token->type = TOKEN_EOL;

	return token;
}


struct token *lexer_next(struct lexer *lexer)
{
	struct token *token;
	mcc_error_t err;
	struct strbuf errbuf;

next_nonwhite_char:
	if (lexer_is_eol(lexer)) {
		err = lexer_read_line(lexer);
		lexer->next_at_bol = true;

		if (err == MCC_ERROR_EOF)
			return &eof;

		if (err != MCC_ERROR_OK)
			return NULL;

		if (!lexer->first_token && lexer->emit_eols)
			return new_eol(lexer);
	}

	lexer->first_token = false;

	eat_whitespace(lexer);
	if (lexer_is_eol(lexer))
		goto next_nonwhite_char;

	token = objpool_alloc(&lexer->cpp->token_pool);
	if (!token)
		return NULL;

	token->is_at_bol = lexer->next_at_bol;
	token->preceded_by_whitespace = lexer->had_whitespace;
	token->noexpand = false;

	lexer->next_at_bol = false;
	lexer->had_whitespace = false;

	/* TODO unhack this */
	lexer->location.column_no = lexer->c - strbuf_get_string(&lexer->linebuf);
	token->startloc = lexer->location;
	/* TODO unhack this */
	token->startloc.filename = lexer->filename;

	lexer->c++;
	switch (lexer->c[-1]) {
	case 'u':
		/* u8"string" */
		if (*lexer->c == '8' && lexer->c[1] == '\"') {
			lexer->c += 2;
			return lexer_lex_string(lexer, token);
		}

	case 'U':
	case 'L':
		/* L'char' or U'char' or u'char' */
		if (*lexer->c == '\'') {
			lexer->c++;
			return lexer_lex_char(lexer, token);
		}
		
		/* L"string" or U"string" or u"string" */
		if (*lexer->c == '\"') {
			lexer->c++;
			return lexer_lex_string(lexer, token);
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
		return lexer_lex_name(lexer, token);

	case '\"':
		if (lexer->inside_include)
			return lexer_lex_header_qname(lexer, token);
		else
			return lexer_lex_string(lexer, token);

	case '\'':
		return lexer_lex_char(lexer, token);

	case '.':
		if (!is_digit(*lexer->c)) {
			if (*lexer->c == '.' && lexer->c[1] == '.') {
				token->type = TOKEN_ELLIPSIS;
				lexer->c += 2;
			}
			else {
				token->type = TOKEN_DOT;
			}

			break;
		}

		/* fall-through to pp-number processing */ 

	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9':
		lexer->c--;
		return lexer_lex_pp_number(lexer, token);

	case '[':
		token->type = TOKEN_LBRACKET;
		break;

	case ']':
		token->type = TOKEN_RBRACKET;
		break;

	case '(':
		token->type = TOKEN_LPAREN;
		break;

	case ')':
		token->type = TOKEN_RPAREN;
		break;

	case '{':
		token->type = TOKEN_LBRACE;
		break;

	case '}':
		token->type = TOKEN_RBRACE;
		break;

	case '-':
		if (*lexer->c == '>') {
			token->type = TOKEN_ARROW;
			lexer->c++;
		}
		else if (*lexer->c == '-') {
			token->type = TOKEN_DEC;
			lexer->c++;
		}
		else if (*lexer->c == '=') {
			token->type = TOKEN_MINUS_EQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_MINUS;
		}

		break;

	case '+':
		if (*lexer->c == '+') {
			token->type = TOKEN_INC;
			lexer->c++;
		}
		else if (*lexer->c == '=') {
			token->type = TOKEN_PLUS_EQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_PLUS;
		}

		break;

	case '&':
		if (*lexer->c == '=') {
			token->type = TOKEN_AND_EQ;
			lexer->c++;
		}
		else if (*lexer->c == '&') {
			token->type = TOKEN_LOGICAL_AND;
			lexer->c++;
		}
		else {
			token->type = TOKEN_AMPERSAND;
		}

		break;

	case '*':
		if (*lexer->c == '=') {
			token->type = TOKEN_MUL_EQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_ASTERISK;
		}

		break;

	case '~':
		token->type = TOKEN_NEG;
		break;

	case '!':
		if (*lexer->c == '=') {
			token->type = TOKEN_NEQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_NOT;
		}

		break;

	case '/':
		if (*lexer->c == '*') {
			lexer->c++;
			eat_c_comment(lexer);
			goto next_nonwhite_char;
		}

		if (*lexer->c == '/') {
			lexer->c++;
			eat_cpp_comment(lexer);
			goto next_nonwhite_char;
		}

		if (*lexer->c == '=') {
			token->type = TOKEN_DIV_EQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_DIV;
		}

		break;

	case '%':
		if (*lexer->c == '=') {
			token->type = TOKEN_MOD_EQ;
			lexer->c++;
		}
		else if (*lexer->c == ':') {
			if (lexer->c[1] == '%' && lexer->c[2] == ':') {
				token->type = TOKEN_HASH_HASH;
				lexer->c += 3;
			}
			else {
				token->type = TOKEN_HASH;
				lexer->c++;
			}
		}
		else if (*lexer->c == '>') {
			token->type = TOKEN_RBRACE;
			lexer->c++;
		}
		else {
			token->type = TOKEN_MOD;
		}
		break;

	case '<':
		if (!lexer->inside_include) {
			if (*lexer->c == '=') {
				lexer->c++;
				token->type = TOKEN_LE;
			}
			else if (*lexer->c == '<') {
				if (lexer->c[1] == '=') {
					token->type = TOKEN_SHL_EQ;
					lexer->c += 2;
				}
				else {
					token->type = TOKEN_SHL;
					lexer->c++;
				}
			}
			else if (*lexer->c == '%') {
				token->type = TOKEN_LBRACE;
				lexer->c++;
			}
			else if (*lexer->c == ':') {
				token->type = TOKEN_LBRACKET;
				lexer->c++;
			}
			else {
				token->type = TOKEN_LT;
			}
		}
		else {
			return lexer_lex_header_hname(lexer, token);
		}
		break;

	case '>':
		if (*lexer->c == '=') {
			token->type = TOKEN_GE;
			lexer->c++;
		}
		else if (*lexer->c == '>') {
			if (lexer->c[1] == '=') {
				token->type = TOKEN_SHR_EQ;
				lexer->c += 2;
			}
			else {
				token->type = TOKEN_SHR;
				lexer->c++;
			}
		}
		else {
			token->type = TOKEN_GT;
		}

		break;

	case '^':
		if (*lexer->c == '=') {
			token->type = TOKEN_XOR_EQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_XOR;
		}

		break;

	case '|':
		if (*lexer->c == '|') {
			token->type = TOKEN_LOGICAL_OR;
			lexer->c++;
		}
		else if (*lexer->c == '=') {
			token->type = TOKEN_OR_EQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_OR;
		}

		break;
	
	case '?':
		token->type = TOKEN_QUESTION_MARK;
		break;

	case ':':
		if (*lexer->c == '>') {
			token->type = TOKEN_RBRACKET;
			lexer->c++;
		}
		else {
			token->type = TOKEN_COLON;
		}

		break;

	case ';':
		token->type = TOKEN_SEMICOLON;
		break;

	case '=':
		if (*lexer->c == '=') {
			token->type = TOKEN_EQ_EQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_EQ;
		}

		break;

	case ',':
		token->type = TOKEN_COMMA;
		break;

	case '#':
		if (*lexer->c == '#') {
			token->type = TOKEN_HASH_HASH;
			lexer->c++;
		}
		else {
			token->type = TOKEN_HASH;
		}

		break;

	default:
		strbuf_init(&errbuf, 4); // TODO get rid of the strbuf
		print_char(*(lexer->c - 1), &errbuf);
		lexer_error(lexer, "character %s was unexpected",
			strbuf_get_string(&errbuf));
		lexer_error(lexer, "line len was %lu", strbuf_strlen(&lexer->linebuf));
		strbuf_free(&errbuf);

		lexer->c++;
		goto next_nonwhite_char;
	}

	return token;
}
