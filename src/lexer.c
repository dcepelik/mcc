#include "debug.h"
#include "lexer.h"
#include "symbol.h"
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define INBUF_BLOCK_SIZE	2048
#define STRBUF_INIT_SIZE	128

static struct token eol = { .type = TOKEN_EOL };
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


mcc_error_t lexer_init(struct lexer *lexer, char *filename)
{
	mcc_error_t err;

	if (!strbuf_init(&lexer->linebuf, STRBUF_INIT_SIZE))
		return MCC_ERROR_NOMEM;

	if (!strbuf_init(&lexer->strbuf, STRBUF_INIT_SIZE)) {
		strbuf_free(&lexer->linebuf);
		return MCC_ERROR_NOMEM;
	}

	err = inbuf_open(&lexer->inbuf, INBUF_BLOCK_SIZE, filename);
	if (err != MCC_ERROR_OK)
		goto out_err;

	lexer->c = strbuf_get_string(&lexer->linebuf);

	lexer->location.line_no = 0;
	lexer->inside_include = false;
	lexer->next_at_bol = true;
	lexer->first_token = true;
	lexer->had_whitespace = false;

	return MCC_ERROR_OK;

out_err:
	strbuf_free(&lexer->linebuf);
	strbuf_free(&lexer->strbuf);

	return err;
}


void lexer_free(struct lexer *lexer)
{
	strbuf_free(&lexer->linebuf);
	strbuf_free(&lexer->strbuf);
	inbuf_close(&lexer->inbuf);
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


struct symbol *lexer_search_or_insert_symbol(struct cpp *file, char *name)
{
	struct symbol *symbol;
	
	symbol = symtab_search(file->symtab, name);
	if (!symbol)
		symbol = symtab_insert(file->symtab, name);

	return symbol;
}


struct token *lexer_lex_name(struct cpp *file, struct lexer *lexer, struct token *token)
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
	token->symbol = lexer_search_or_insert_symbol(file, strbuf_get_string(&lexer->strbuf));

	if (!token->symbol)
		return NULL;

	return token;
}


static struct token *lexer_lex_char(struct cpp *file, struct lexer *lexer, struct token *token)
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

	token->type = TOKEN_CHAR_CONST;
	token->value = val;

	return token;
}


static inline bool lexer_is_eol(struct lexer *lexer)
{
	return (lexer->c - strbuf_get_string(&lexer->linebuf))
		>= strbuf_strlen(&lexer->linebuf);
}


struct token *lexer_lex_pp_number(struct cpp *file, struct lexer *lexer, struct token *token)
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
	token->str = strbuf_copy_to_mempool(&lexer->strbuf, &file->token_data);

	if (!token->str)
		return NULL;

	return token;
}


struct token *lexer_lex_string(struct cpp *file, struct lexer *lexer, struct token *token)
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

	token->type = TOKEN_STRING;
	token->str = strbuf_copy_to_mempool(&lexer->strbuf, &file->token_data);

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


struct token *lexer_lex_header_name(struct cpp *file, struct lexer *lexer, struct token *token,
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
			DEBUG_PRINTF("error: unexpected char: %c", c);
		}
		else {
			strbuf_putc(&lexer->strbuf, c);
		}
	}

	if (!seen_delim)
		DEBUG_PRINTF("error: %c was expected here", delim);

	// TODO Check (and warn about) presence of // and /* comments within header name

	token->type = TOKEN_HEADER_NAME;
	token->str = strbuf_copy_to_mempool(&lexer->strbuf, &file->token_data);

	if (!token->str)
		return NULL;

	return token;
}


struct token *lexer_lex_header_hname(struct cpp *file, struct lexer *lexer, struct token *token)
{
	return lexer_lex_header_name(file, lexer, token, is_valid_hchar, '>');
}


struct token *lexer_lex_header_qname(struct cpp *file, struct lexer *lexer, struct token *token)
{
	return lexer_lex_header_name(file, lexer, token, is_valid_qchar, '\"');
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

	lexer->location.line_no++;

	if (c == INBUF_EOF)
		return MCC_ERROR_EOF;

eol_or_eof:
	lexer->c = strbuf_get_string(&lexer->linebuf);

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


struct token *lexer_next(struct cpp *file, struct lexer *lexer)
{
	struct token *token;
	mcc_error_t err;

next_nonwhite_char:
	if (lexer_is_eol(lexer)) {
		err = lexer_read_line(lexer);
		lexer->next_at_bol = true;

		if (err == MCC_ERROR_EOF)
			return &eof;

		if (err != MCC_ERROR_OK)
			return NULL;

		if (!lexer->first_token)
			return &eol;
	}

	lexer->first_token = false;

	eat_whitespace(lexer);

	token = objpool_alloc(&file->token_pool);
	if (!token)
		return NULL;

	token->is_at_bol = lexer->next_at_bol;
	token->preceded_by_whitespace = lexer->had_whitespace;

	lexer->next_at_bol = false;
	lexer->had_whitespace = false;

	/* TODO unhack this */
	lexer->location.column_no = lexer->c - strbuf_get_string(&lexer->linebuf);
	token->startloc = lexer->location;

	lexer->c++;
	switch (lexer->c[-1]) {
	case 'u':
		/* u8"string" */
		if (*lexer->c == '8' && lexer->c[1] == '\"') {
			lexer->c += 2;
			return lexer_lex_string(file, lexer, token);
		}

	case 'U':
	case 'L':
		/* L'char' or U'char' or u'char' */
		if (*lexer->c == '\'') {
			lexer->c++;
			return lexer_lex_char(file, lexer, token);
		}
		
		/* L"string" or U"string" or u"string" */
		if (*lexer->c == '\"') {
			lexer->c++;
			return lexer_lex_string(file, lexer, token);
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
		return lexer_lex_name(file, lexer, token);

	case '\"':
		if (lexer->inside_include)
			return lexer_lex_header_qname(file, lexer, token);
		else
			return lexer_lex_string(file, lexer, token);

	case '\'':
		return lexer_lex_char(file, lexer, token);

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
		return lexer_lex_pp_number(file, lexer, token);

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
			return lexer_lex_header_hname(file, lexer, token);
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
		DEBUG_PRINTF("error: character %c was unexpected", *lexer->c);
		lexer->c++;
		goto next_nonwhite_char;
	}

	return token;
}
