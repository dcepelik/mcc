#include "context.h"
#include "debug.h"
#include "lexer.h"
#include "print.h"
#include "symbol.h"
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define STRBUF_INIT_SIZE	128

/*
 * Single-char escape sequences: mapping from what follows the \ to the resulting character.
 */
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

/*
 * See 5.2.1.1 Trigraph sequences
 */
static const char trigraph[256] = {
	['='] = '#',
	['('] = '[',
	['/'] = '\\',
	[')'] = ']',
	['\''] = '^',
	['<'] = '{',
	['!'] = '|',
	['>'] = '}',
	['-'] = '~',
};

void lexer_init(struct lexer *lexer, struct context *ctx, struct inbuf *inbuf)
{
	strbuf_init(&lexer->linebuf, STRBUF_INIT_SIZE);
	strbuf_init(&lexer->strbuf, STRBUF_INIT_SIZE);
	strbuf_init(&lexer->spelling, STRBUF_INIT_SIZE);

	lexer->ctx = ctx;
	lexer->inbuf = inbuf;

	lexer->c = strbuf_get_string(&lexer->linebuf);
	lexer->location.line_no = 0;
	lexer->inside_include = false;
	lexer->next_at_bol = true;
	lexer->first_token = true;
	lexer->had_whitespace = false;

	lexer->filename = NULL; /* TODO */
}

void lexer_free(struct lexer *lexer)
{
	strbuf_free(&lexer->linebuf);
	strbuf_free(&lexer->strbuf);
	strbuf_free(&lexer->spelling);
}

static void lexer_error_internal(struct lexer *lexer, enum error_level level,
	char *fmt, va_list args, char *context, size_t context_len,
	struct location location)
{
	struct strbuf msg;

	strbuf_init(&msg, 128);
	strbuf_vprintf_at(&msg, 0, fmt, args);

	errlist_insert(&lexer->ctx->errlist,
		level,
		lexer->filename,
		strbuf_get_string(&msg),
		context,
		context_len,
		location);

	strbuf_free(&msg);
}

static void lexer_error(struct lexer *lexer, char *fmt, ...)
{
	va_list args;

	/* TODO unhack this */
	lexer->location.column_no = lexer->c - strbuf_get_string(&lexer->linebuf);

	va_start(args, fmt);
	lexer_error_internal(lexer, ERROR_LEVEL_ERROR, fmt, args,
		strbuf_get_string(&lexer->linebuf), strbuf_strlen(&lexer->linebuf) + 1, // TODO
		lexer->location);
	va_end(args);
}

static void lexer_error_noctx(struct lexer *lexer, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	lexer_error_internal(lexer, ERROR_LEVEL_ERROR, fmt, args, NULL, 0,
		lexer->location);
	va_end(args);
}

static inline void lexer_spelling_start(struct lexer *lexer)
{
	lexer->spelling_start = lexer->c;
}

static inline char *lexer_spelling_end(struct lexer *lexer)
{
	assert(lexer->c >= lexer->spelling_start);

	char *c;

	strbuf_reset(&lexer->spelling);
	for (c = lexer->spelling_start; c < lexer->c; c++)
		strbuf_putc(&lexer->spelling, *c);
	
	return strbuf_copy_to_mempool(&lexer->spelling, &lexer->ctx->token_data);
}

inline static bool is_ascii_letter(int c)
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
	size_t i;

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

static uint64_t lexer_read_ucn(struct lexer *lexer)
{
	uint32_t number;

	switch (*lexer->c++) {
		case 'u':
			number = read_hex_number(lexer, 4, 4);
			break;

		case 'U':
			number = read_hex_number(lexer, 8, 8);
			break;

		default:
			assert(false);
	}

	if (number < 0x00A0 && number != 0x0024 && number != 0x0040 && number != 0x0060) {
		lexer_error(lexer, "invalid UCN character name: less than 0x00A0 and none of @, $ or `");
		/* TODO make that error fatal? */
	}
	if (number >= 0xD800 && number <= 0xDFFF) {
		lexer_error(lexer, "invalid UCN character name: in the range D800 through DFFF inclusive");
	}

	return number;
}

static struct token *lexer_lex_name(struct lexer *lexer, struct token *token)
{
	strbuf_reset(&lexer->strbuf);

	while (is_ascii_letter(*lexer->c) || is_digit(*lexer->c) || *lexer->c == '_' || *lexer->c == '\\') {
		if (*lexer->c == '\\') {
			DEBUG_TRACE;
			if (lexer->c[1] == 'u' || lexer->c[1] == 'U') {
				lexer->c++;
				strbuf_putwc(&lexer->strbuf, lexer_read_ucn(lexer));
			}
			else {
				break;
			}
		}
		else {
			strbuf_putc(&lexer->strbuf, *lexer->c);
			lexer->c++;
		}
	}

	token->type = TOKEN_NAME;
	token->symbol = symtab_find_or_insert(&lexer->ctx->symtab, strbuf_get_string(&lexer->strbuf));
	token->spelling = lexer_spelling_end(lexer);

	return token;
}

static struct token *lexer_lex_char(struct lexer *lexer, struct token *token)
{
	uint32_t val = 0;
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
	token->spelling = lexer_spelling_end(lexer);

	return token;
}

static inline bool lexer_is_eol(struct lexer *lexer)
{
	return (size_t)(lexer->c - strbuf_get_string(&lexer->linebuf))
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
			if (c != 'e' && c != 'E' && c != 'p' && c != 'P')
				break;
		}
		else if (*lexer->c == '\\') {
			if (lexer->c[1] == 'u' || lexer->c[1] == 'U') {
				lexer->c++;
				strbuf_putwc(&lexer->strbuf, lexer_read_ucn(lexer));
				continue;
			}
			else {
				break;
			}

		}
		else if (!is_digit(*lexer->c) && !is_ascii_letter(*lexer->c) && *lexer->c != '.' && *lexer->c != '_') {
			break;
		}

		strbuf_putc(&lexer->strbuf, *lexer->c);
		lexer->c++;
	}

	token->type = TOKEN_NUMBER;
	token->str = strbuf_copy_to_mempool(&lexer->strbuf, &lexer->ctx->token_data);
	token->spelling = lexer_spelling_end(lexer);

	return token;
}

struct token *lexer_lex_string_literal(struct lexer *lexer, struct token *token)
{
	bool seen_delim = false;
	size_t i;

	strbuf_reset(&lexer->strbuf);
	assert(lexer->c[-1] == '\"');

	for (i = 0; !lexer_is_eol(lexer); i++) {
		if (*lexer->c == '\"') {
			seen_delim = true;
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

	if (!seen_delim)
		lexer_error(lexer, "missing the final \"");

	token->type = TOKEN_STRING_LITERAL;
	token->lstr.str = strbuf_copy_to_mempool(&lexer->strbuf, &lexer->ctx->token_data);
	token->lstr.len = strbuf_strlen(&lexer->strbuf);
	token->spelling = lexer_spelling_end(lexer);

	return token;
}

struct token *lexer_lex_header_name(struct lexer *lexer, struct token *token, bool (*is_valid_char)(char c), char delim)
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

	token->str = strbuf_copy_to_mempool(&lexer->strbuf, &lexer->ctx->token_data);
	token->spelling = lexer_spelling_end(lexer);

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
 * TODO Refactor, don't use mcc_error_t to signalize EOF
 */
static mcc_error_t lexer_read_line(struct lexer *lexer)
{
	int c;
	bool escape = false;
	size_t num_qmarks = 0;	/* number of consecutive question-marks '?' */

	strbuf_reset(&lexer->linebuf);

	while ((c = inbuf_get_char(lexer->inbuf)) != INBUF_EOF) {
		/* basically Aho-Corasick matcher for trigraph sequences */
		if (num_qmarks == 2) {
			if (!trigraph[c]) {
				if (c == '?') {
					strbuf_putc(&lexer->linebuf, '?');
					continue;
				}

				strbuf_printf(&lexer->linebuf, "??");
				num_qmarks = 0;
			}
			else {
				c = trigraph[c];
				num_qmarks = 0;
			}
		}

		if (c == '?') {
			num_qmarks++;
			continue;
		}
		else {
			if (num_qmarks > 0) {
				assert(num_qmarks == 1);
				strbuf_printf(&lexer->linebuf, "?");
			}

			num_qmarks = 0;
		}

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

			/* fall-through */

		default:
			if (escape) {
				strbuf_putc(&lexer->linebuf, '\\');
			}

			strbuf_putc(&lexer->linebuf, c);
			escape = false;
		}
	}

	if (strbuf_strlen(&lexer->linebuf) == 0)
		return MCC_ERROR_EOF;

eol_or_eof:
	lexer->c = strbuf_get_string(&lexer->linebuf);
	lexer->location.line_no++;
	lexer->location.column_no = 0;

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
		if (*lexer->c == '*' && lexer->c[1] == '/') {
			lexer->c += 2;
			return;
		}

		lexer->c++;
	}

	if (lexer_read_line(lexer) != MCC_ERROR_EOF)
		goto search_comment_terminator;

	lexer_error_noctx(lexer, "missing */");
}

/*
 * This is a utility function used by lexer_next.
 */
static inline enum enc_prefix get_enc_prefix(char c)
{
	assert(c == 'u' || c == 'U' || c == 'L');

	if (c == 'u')
		return ENC_PREFIX_U;
	else if (c == 'U')
		return ENC_PREFIX_UPPER_U;
	else if (c == 'L')
		return ENC_PREFIX_L;

	return ENC_PREFIX_NONE;
}

void lexer_next(struct lexer *lexer, struct token *token)
{
	mcc_error_t err;

next_nonwhite_char:
	if (lexer_is_eol(lexer)) {
		err = lexer_read_line(lexer);
		lexer->next_at_bol = true;

		if (err == MCC_ERROR_EOF) {
			token->type = TOKEN_EOF;
			token->is_at_bol = false;
			return;
		}

		assert(err == MCC_ERROR_OK);
	}

	lexer->first_token = false; /* move down? */

	eat_whitespace(lexer);
	if (lexer_is_eol(lexer))
		goto next_nonwhite_char;

	token->is_at_bol = lexer->next_at_bol;
	token->after_white = lexer->had_whitespace;
	token->noexpand = false;
	token->enc_prefix = ENC_PREFIX_NONE;

	lexer->next_at_bol = false;
	lexer->had_whitespace = false;

	/* TODO unhack this */
	lexer->location.column_no = lexer->c - strbuf_get_string(&lexer->linebuf);
	token->startloc = lexer->location;
	/* TODO unhack this */
	token->startloc.filename = lexer->filename;

	lexer_spelling_start(lexer);

	lexer->c++;
	switch (lexer->c[-1]) {
	case 'u':
		/* u8"string" */
		if (*lexer->c == '8' && lexer->c[1] == '\"') {
			lexer->c += 2;
			token->enc_prefix = ENC_PREFIX_U8;
			lexer_lex_string_literal(lexer, token);
			return;
		}

		/* fall-through: it's not a string prefix */

	case 'U':
		/* fall-through */
	case 'L':
		/* L'char' or U'char' or u'char' */
		if (*lexer->c == '\'') {
			token->enc_prefix = get_enc_prefix(lexer->c[-1]);
			lexer->c++;
			lexer_lex_char(lexer, token);
			return;
		}
		
		/* L"string" or U"string" or u"string" */
		if (*lexer->c == '\"') {
			token->enc_prefix = get_enc_prefix(lexer->c[-1]);
			lexer->c++;
			lexer_lex_string_literal(lexer, token);
			return;
		}

		/* fall-through: it's just a regular letter, not a prefix */

	case '_': case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm':
	case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
	case 'v': case 'w': case 'x': case 'y': case 'z': case 'A': case 'B':
	case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I':
	case 'J': case 'K': case 'M': case 'N': case 'O': case 'P': case 'Q':
	case 'R': case 'S': case 'T': case 'V': case 'W': case 'X': case 'Y':
	case 'Z':
		lexer->c--;
		lexer_lex_name(lexer, token);
		return;

	case '\"':
		if (lexer->inside_include)
			lexer_lex_header_qname(lexer, token);
		else
			lexer_lex_string_literal(lexer, token);
		return;

	case '\'':
		lexer_lex_char(lexer, token);
		return;

	case '.':
		if (!is_digit(*lexer->c)) {
			if (*lexer->c == '.' && lexer->c[1] == '.') {
				token->type = TOKEN_ELLIPSIS;
				lexer->c += 2;
			}
			else {
				token->type = TOKEN_OP_DOT;
			}

			break;
		}

		/* decimal dot, fall-through to pp-number processing */ 

	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9':
		lexer->c--;
		lexer_lex_pp_number(lexer, token);
		return;

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
			token->type = TOKEN_OP_ARROW;
			lexer->c++;
		}
		else if (*lexer->c == '-') {
			token->type = TOKEN_MINUSMINUS;
			lexer->c++;
		}
		else if (*lexer->c == '=') {
			token->type = TOKEN_OP_SUBEQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_MINUS;
		}

		break;

	case '+':
		if (*lexer->c == '+') {
			token->type = TOKEN_PLUSPLUS;
			lexer->c++;
		}
		else if (*lexer->c == '=') {
			token->type = TOKEN_OP_ADDEQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_PLUS;
		}

		break;

	case '&':
		if (*lexer->c == '=') {
			token->type = TOKEN_OP_BITANDEQ;
			lexer->c++;
		}
		else if (*lexer->c == '&') {
			token->type = TOKEN_OP_AND;
			lexer->c++;
		}
		else {
			token->type = TOKEN_AMP;
		}

		break;

	case '*':
		if (*lexer->c == '=') {
			token->type = TOKEN_OP_MULEQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_ASTERISK;
		}

		break;

	case '~':
		token->type = TOKEN_OP_NEG;
		break;

	case '!':
		if (*lexer->c == '=') {
			token->type = TOKEN_OP_NEQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_OP_NOT;
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
			token->type = TOKEN_OP_DIVEQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_OP_DIV;
		}

		break;

	case '%':
		if (*lexer->c == '=') {
			token->type = TOKEN_OP_MODEQ;
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
			token->type = TOKEN_OP_MOD;
		}
		break;

	case '<':
		if (!lexer->inside_include) {
			if (*lexer->c == '=') {
				lexer->c++;
				token->type = TOKEN_OP_LE;
			}
			else if (*lexer->c == '<') {
				if (lexer->c[1] == '=') {
					token->type = TOKEN_OP_SHLEQ;
					lexer->c += 2;
				}
				else {
					token->type = TOKEN_OP_SHL;
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
				token->type = TOKEN_OP_LT;
			}
		}
		else {
			lexer_lex_header_hname(lexer, token);
			return;
		}
		break;

	case '>':
		if (*lexer->c == '=') {
			token->type = TOKEN_OP_GE;
			lexer->c++;
		}
		else if (*lexer->c == '>') {
			if (lexer->c[1] == '=') {
				token->type = TOKEN_OP_SHREQ;
				lexer->c += 2;
			}
			else {
				token->type = TOKEN_OP_SHR;
				lexer->c++;
			}
		}
		else {
			token->type = TOKEN_OP_GT;
		}

		break;

	case '^':
		if (*lexer->c == '=') {
			token->type = TOKEN_OP_XOREQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_OP_XOR;
		}

		break;

	case '|':
		if (*lexer->c == '|') {
			token->type = TOKEN_OP_OR;
			lexer->c++;
		}
		else if (*lexer->c == '=') {
			token->type = TOKEN_OP_BITOREQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_OP_BITOR;
		}

		break;
	
	case '?':
		token->type = TOKEN_QMARK;
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
			token->type = TOKEN_OP_EQ;
			lexer->c++;
		}
		else {
			token->type = TOKEN_OP_ASSIGN;
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

	case '\\':
		if (*lexer->c == 'u' || *lexer->c == 'U') {
			lexer_lex_name(lexer, token);
			return;
		}
		
		/* fall through to default lexeme handling */

	default:
		token->type = TOKEN_OTHER;
		token->value = lexer->c[-1];
		token->spelling = lexer_spelling_end(lexer);
	}
}
