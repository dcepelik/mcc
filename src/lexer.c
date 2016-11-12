#include "debug.h"
#include "lexer.h"
#include "symtab.h"
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>


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

	if (i == 0)
		DEBUG_MSG("error: octal digit expected");

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

	switch (*lexer->c) {
	case 'a':
		lexer->c++;
		return '\a';

	case 'b':
		lexer->c++;
		return '\b';

	case 'f':
		lexer->c++;
		return '\f';

	case 'r':
		lexer->c++;
		return '\r';

	case 'n':
		lexer->c++;
		return '\n';

	case 'v':
		lexer->c++;
		return '\v';

	case 't':
		lexer->c++;
		return '\t';

	case '\\':
		lexer->c++;
		return '\\';

	case '\"':
		lexer->c++;
		return '\"';

	case '\'':
		lexer->c++;
		return '\'';

	case '?':
		lexer->c++;
		return '\?';

	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
		return read_octal_number(lexer);

	case 'x':	
		lexer->c++;
		return read_hex_number(lexer, 1, 8);

	case 'u':
		lexer->c++;
		return read_hex_number(lexer, 4, 4);

	case 'U':
		lexer->c++;
		return read_hex_number(lexer, 8, 8);

	default:
		DEBUG_PRINTF("error: unknown escape sequence: \\%c", *lexer->c);
		lexer->c++;
		return -1;
	}
}


mcc_error_t lex_identifier_or_keyword(struct lexer *lexer, struct token_data *token)
{
	size_t i = 0;

	while (is_letter(*lexer->c) || is_digit(*lexer->c) || *lexer->c == '_' || *lexer->c == '\\') {
		token->ident[i++] = *lexer->c;
		lexer->c++;
	}

	token->ident[i] = '\0';

	struct symbol *symbol = symtab_search(lexer->symtab, token->ident);
	if (symbol != NULL && symbol->type == SYMBOL_KEYWORD) {
		token->token = symbol->keyword;
	}
	else
		token->token = TOKEN_IDENT;

	return MCC_ERROR_OK;

}


static mcc_error_t lex_char_const(struct lexer *lexer, struct token_data *token)
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

	token->value = val;
	token->token = TOKEN_CHAR_CONST;

	return MCC_ERROR_OK;
}


mcc_error_t lex_pp_number(struct lexer *lexer, struct token_data *token)
{
	lexer->c++;
	return MCC_ERROR_OK;
}


mcc_error_t lex_string_literal(struct lexer *lexer, struct token_data *token)
{
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
			token->string[i] = read_escape_sequence(lexer);
		}
		else {
			token->string[i] = *lexer->c;
			lexer->c++;
		}
	}

	if (!seen_delimiter) {
		DEBUG_MSG("error: missing the final \"");
	}

	token->string[i] = '\0';
	token->token = TOKEN_STRING_LITERAL;

	return MCC_ERROR_OK;
}


#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof(*arr))


void setup_symbol_table(struct lexer *lexer)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(keywords); i++) {
		struct symbol *s = symtab_insert(lexer->symtab, token_names[keywords[i]]);
		s->type = SYMBOL_KEYWORD;
		s->keyword = keywords[i];
	}
}


mcc_error_t lexer_init(struct lexer *lexer)
{
	lexer->buffer = NULL;

	lexer->line_size = 1024; // TODO
	lexer->line = malloc(lexer->line_size);
	if (!lexer->line)
		return MCC_ERROR_NOMEM;

	lexer->line[0] = '\0';
	lexer->c = lexer->line; // TODO 

	return MCC_ERROR_OK;
}


void lexer_set_buffer(struct lexer *lexer, struct inbuf *buffer)
{
	assert(lexer->buffer == NULL);

	lexer->buffer = buffer;

	lexer->line_no = 0;
	lexer->col_no = 0;
	lexer->line_len = 0;
	lexer->c = lexer->line;
}


void lexer_set_symtab(struct lexer *lexer, struct symtab *symtab)
{
	lexer->symtab = symtab;
	setup_symbol_table(lexer);
}


void lexer_free(struct lexer *lexer)
{
	free(lexer->line);
}


static mcc_error_t lexer_read_line(struct lexer *lexer)
{
	int c;
	size_t i = 0;
	bool escape = false;

	assert(lexer->buffer != NULL);

	for (lexer->col_no = 0; (c = inbuf_get_char(lexer->buffer)) != INBUF_EOF; lexer->col_no++) {
		if (i + 2 >= lexer->line_size) {
			lexer->line = realloc(lexer->line, 2 * lexer->line_size);

			if (!lexer->line)
				return MCC_ERROR_NOMEM;
		}

		switch (c) {
		case ' ':
		case '\t':
		case '\v':
		// TODO more cases
		case '\f':
			lexer->line[i++] = c;
			continue;

		case '\n':
			lexer->line_no++;

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
				lexer->line[i++] = '\\';
			}

			lexer->line[i++] = c;
			escape = false;
		}
	}

	if (c == INBUF_EOF)
		return MCC_ERROR_EOF;

eol_or_eof:
	lexer->line[i] = '\0';
	lexer->line_len = i;
	lexer->c = lexer->line;

	return MCC_ERROR_OK;
}


static inline void eat_whitespace(struct lexer *lexer)
{
	while (is_whitespace(*lexer->c))
		lexer->c++;
}


static inline bool lexer_is_eol(struct lexer *lexer)
{
	return (lexer->c - lexer->line) >= lexer->line_len;
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


mcc_error_t lexer_next(struct lexer *lexer, struct token_data *token)
{
	mcc_error_t err;

smash_whitespace:
	eat_whitespace(lexer);
	
	if (lexer_is_eol(lexer)) {
		err = lexer_read_line(lexer);

		if (err == MCC_ERROR_OK)
			goto smash_whitespace;
		else
			return err;
	}

	lexer->c++;

	switch (lexer->c[-1]) {
	case 'u':
		/* u8"string" */
		if (*lexer->c == '8' && lexer->c[1] == '\"') {
			lexer->c += 2;
			return lex_string_literal(lexer, token);
		}

	case 'U':
	case 'L':
		/* L'char' or U'char' or u'char' */
		if (*lexer->c == '\'') {
			lexer->c++;
			return lex_char_const(lexer, token);
		}
		
		/* L"string" or U"string" or u"string" */
		if (*lexer->c == '\"') {
			lexer->c++;
			return lex_string_literal(lexer, token);
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
		return lex_identifier_or_keyword(lexer, token);

	case '\"':
		return lex_string_literal(lexer, token);

	case '\'':
		return lex_char_const(lexer, token);

	case '.':
		if (!is_digit(lexer->c[1])) {
			if (*lexer->c == '.' && lexer->c[1] == '.') {
				token->token = TOKEN_ELLIPSIS;
				lexer->c += 2;
			}
			else {
				token->token = TOKEN_DOT;
			}

			break;
		}

		/* fall-through to number processing */ 

	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9':
		return lex_pp_number(lexer, token);

	case '[':
		token->token = TOKEN_LBRACKET;
		break;

	case ']':
		token->token = TOKEN_RBRACKET;
		break;

	case '(':
		token->token = TOKEN_LPAREN;
		break;

	case ')':
		token->token = TOKEN_RPAREN;
		break;

	case '{':
		token->token = TOKEN_LBRACE;
		break;

	case '}':
		token->token = TOKEN_RBRACE;
		break;

	case '-':
		if (*lexer->c == '>') {
			token->token = TOKEN_ARROW;
			lexer->c++;
		}
		else if (*lexer->c == '-') {
			token->token = TOKEN_DEC;
			lexer->c++;
		}
		else if (*lexer->c == '=') {
			token->token = TOKEN_MINUS_EQ;
			lexer->c++;
		}
		else {
			token->token = TOKEN_MINUS;
		}

		break;

	case '+':
		if (*lexer->c == '+') {
			token->token = TOKEN_INC;
			lexer->c++;
		}
		else if (*lexer->c == '=') {
			token->token = TOKEN_PLUS_EQ;
			lexer->c++;
		}
		else {
			token->token = TOKEN_PLUS;
		}

		break;

	case '&':
		if (*lexer->c == '=') {
			token->token = TOKEN_AND_EQ;
			lexer->c++;
		}
		else if (*lexer->c == '&') {
			token->token = TOKEN_LOGICAL_AND;
			lexer->c++;
		}
		else {
			token->token = TOKEN_AMPERSAND;
		}

		break;

	case '*':
		if (*lexer->c == '=') {
			token->token = TOKEN_MUL_EQ;
			lexer->c++;
		}
		else {
			token->token = TOKEN_ASTERISK;
		}

		break;

	case '~':
		token->token = TOKEN_NEQ;
		break;

	case '!':
		if (*lexer->c == '=') {
			token->token = TOKEN_NEQ;
			lexer->c++;
		}
		else {
			token->token = TOKEN_NOT;
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
			token->token = TOKEN_DIV_EQ;
			lexer->c++;
		}
		else {
			token->token = TOKEN_DIV;
		}

		break;

	case '%':
		if (*lexer->c == '=') {
			token->token = TOKEN_MOD_EQ;
			lexer->c++;
		}
		else if (*lexer->c == ':') {
			if (lexer->c[1] == '%' && lexer->c[2] == ':') {
				token->token = TOKEN_HASH_HASH;
				lexer->c += 3;
			}
			else {
				token->token = TOKEN_HASH;
				lexer->c++;
			}
		}

	case '<':
		if (*lexer->c == '=') {
			token->token = TOKEN_LE;
		}
		else if (*lexer->c == '<') {
			if (lexer->c[1] == '=') {
				token->token = TOKEN_SHR_EQ;
				lexer->c += 2;
			}
			else {
				token->token = TOKEN_SHR;
				lexer->c++;
			}
		}
		else if (lexer->c[1] == '%') {
			token->token = TOKEN_LBRACE;
			lexer->c++;
		}
		else if (lexer->c[1] == ':') {
			token->token = TOKEN_LBRACKET;
			lexer->c++;
		}
		else {
			token->token = TOKEN_LT;
		}
		break;

	case '>':
		if (*lexer->c == '=') {
			token->token = TOKEN_GE;
			lexer->c++;
		}
		else if (*lexer->c == '>') {
			if (lexer->c[1] == '=') {
				token->token = TOKEN_SHR_EQ;
				lexer->c += 2;
			}
			else {
				token->token = TOKEN_SHR;
				lexer->c++;
			}
		}
		else {
			token->token = TOKEN_GT;
		}

		break;

	case '^':
		if (*lexer->c == '=') {
			token->token = TOKEN_XOR_EQ;
			lexer->c++;
		}
		else {
			token->token = TOKEN_EQ;
		}

		break;

	case '|':
		if (*lexer->c == '|') {
			token->token = TOKEN_LOGICAL_OR;
			lexer->c++;
		}
		else if (*lexer->c == '=') {
			token->token = TOKEN_OR_EQ;
			lexer->c++;
		}
		else {
			token->token = TOKEN_OR;
		}

		break;
	
	case '?':
		token->token = TOKEN_QUESTION_MARK;
		break;

	case ':':
		if (*lexer->c == '>') {
			token->token = TOKEN_RBRACKET;
			lexer->c++;
		}
		else {
			token->token = TOKEN_SEMICOLON;
		}

		break;

	case ';':
		token->token = TOKEN_SEMICOLON;
		break;

	case '=':
		if (*lexer->c == '=') {
			token->token = TOKEN_EQEQ;
			lexer->c++;
		}
		else {
			token->token = TOKEN_EQ;
		}

		break;

	case ',':
		token->token = TOKEN_COMMA;
		break;

	case '#':
		if (lexer->c[1] == '#') {
			token->token = TOKEN_HASH_HASH;
			lexer->c += 2;
		}
		else {
			token->token = TOKEN_HASH;
		}

		break;

	default:
		DEBUG_PRINTF("error: character %c was unexpected", *lexer->c);
		lexer->c++;
	}

	return MCC_ERROR_OK;
}


void lexer_dump_token(struct token_data *token)
{
	const char *name = token_names[token->token];

	switch (token->token) {
	case TOKEN_STRING_LITERAL:
		printf("\"%s\"", token->string);
		break;

	case TOKEN_IDENT:
		printf("%s", token->ident);
		break;

	default:
		printf("%s", name);
	}

	putchar(' ');
}
