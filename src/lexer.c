#include "debug.h"
#include "lexer.h"
#include "symtab.h"
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

static enum token keywords[] = {
	TOKEN_ALIGNAS, TOKEN_ALIGNOF, TOKEN_ATOMIC, TOKEN_BOOL, TOKEN_COMPLEX,
	TOKEN_GENERIC, TOKEN_IMAGINARY, TOKEN_NORETURN, TOKEN_STATIC_ASSERT,
	TOKEN_THREAD_LOCAL, TOKEN_AUTO, TOKEN_BREAK, TOKEN_CASE, TOKEN_CHAR,
	TOKEN_CONST, TOKEN_CONTINUE, TOKEN_DEFAULT, TOKEN_DO, TOKEN_DOUBLE,
	TOKEN_ELSE, TOKEN_ENUM, TOKEN_EXTERN, TOKEN_FLOAT, TOKEN_FOR,
	TOKEN_GOTO, TOKEN_IF, TOKEN_INLINE, TOKEN_INT, TOKEN_LONG,
	TOKEN_REGISTER, TOKEN_RESTRICT, TOKEN_RETURN, TOKEN_SHORT, TOKEN_SIGNED,
	TOKEN_SIZEOF, TOKEN_STATIC, TOKEN_STRUCT, TOKEN_SWITCH, TOKEN_TYPEDEF,
	TOKEN_UNION, TOKEN_UNSIGNED, TOKEN_VOID, TOKEN_VOLATILE, TOKEN_WHILE,
};

static enum token cpp_directives[] = {
	TOKEN_CPP_IF, TOKEN_CPP_IFDEF, TOKEN_CPP_IFNDEF, TOKEN_CPP_ELIF,
	TOKEN_CPP_ELSE, TOKEN_CPP_ENDIF, TOKEN_CPP_INCLUDE, TOKEN_CPP_DEFINE,
	TOKEN_CPP_UNDEF, TOKEN_CPP_LINE, TOKEN_CPP_ERROR, TOKEN_CPP_PRAGMA,
};

static const char *token_names[] = {
	[TOKEN_ALIGNAS] = "_Alignas",
	[TOKEN_ALIGNOF] = "_Alignof",
	[TOKEN_AMPERSAND] = "&",
	[TOKEN_AND_EQ] = "&=",
	[TOKEN_ARROW] = "->",
	[TOKEN_ASTERISK] = "*",
	[TOKEN_ATOMIC] = "_Atomic",
	[TOKEN_AUTO] = "auto",
	[TOKEN_BOOL] = "_Bool",
	[TOKEN_BREAK] = "break",
	[TOKEN_CASE] = "case",
	[TOKEN_CHAR] = "char",
	[TOKEN_CHAR_CONST] = "CHAR_CONST",
	[TOKEN_COLON] = ":",
	[TOKEN_COMMA] = ",",
	[TOKEN_COMPLEX] = "_Complex",
	[TOKEN_CONST] = "const",
	[TOKEN_CONTINUE] = "continue",
	[TOKEN_CPP_DEFINE] = "define",
	[TOKEN_CPP_ELIF] = "elif",
	[TOKEN_CPP_ELSE] = "else",
	[TOKEN_CPP_ENDIF] = "endif",
	[TOKEN_CPP_ERROR] = "error",
	[TOKEN_CPP_IFDEF] = "ifdef",
	[TOKEN_CPP_IFNDEF] = "ifndef",
	[TOKEN_CPP_IF] = "if",
	[TOKEN_CPP_INCLUDE] = "include",
	[TOKEN_CPP_LINE] = "line",
	[TOKEN_CPP_PRAGMA] = "pragma",
	[TOKEN_CPP_UNDEF] = "undef",
	[TOKEN_DEC] = "--",
	[TOKEN_DEFAULT] = "default",
	[TOKEN_DIV] = "/",
	[TOKEN_DIV_EQ] = "/=",
	[TOKEN_DOT] = ".",
	[TOKEN_DOUBLE] = "double",
	[TOKEN_DO] = "do",
	[TOKEN_ELLIPSIS] = "...",
	[TOKEN_ELSE] = "else",
	[TOKEN_ENUM] = "enum",
	[TOKEN_EOF] = "EOF",
	[TOKEN_EQEQ] = "==",
	[TOKEN_EQ] = "=",
	[TOKEN_ERROR] = "!ERROR",
	[TOKEN_EXTERN] = "extern",
	[TOKEN_FLOAT] = "float",
	[TOKEN_FOR] = "for",
	[TOKEN_GENERIC] = "_Generic",
	[TOKEN_GE] = ">=",
	[TOKEN_GOTO] = "goto",
	[TOKEN_GT] = ">",
	[TOKEN_HASH] = "#",
	[TOKEN_HASH_HASH] = "##",
	[TOKEN_HEADER_NAME] = "HEADER_NAME",
	[TOKEN_IDENT] = "IDENT",
	[TOKEN_IF] = "if",
	[TOKEN_IMAGINARY] = "_Imaginary",
	[TOKEN_INC] = "++",
	[TOKEN_INLINE] = "inline",
	[TOKEN_INT] = "int",
	[TOKEN_LBRACE] = "{",
	[TOKEN_LBRACKET] = "[",
	[TOKEN_LE] = "<=",
	[TOKEN_LOGICAL_AND] = "&&",
	[TOKEN_LOGICAL_OR] = "||",
	[TOKEN_LONG] = "long",
	[TOKEN_LPAREN] = "(",
	[TOKEN_LT] = "<",
	[TOKEN_MINUS] = "-",
	[TOKEN_MINUS_EQ] = "-=",
	[TOKEN_MOD] = "%",
	[TOKEN_MOD_EQ] = "%=",
	[TOKEN_MUL_EQ] = "*=",
	[TOKEN_NEG] = "~",
	[TOKEN_NEQ] = "!=",
	[TOKEN_NORETURN] = "_Noreturn",
	[TOKEN_NOT] = "!",
	[TOKEN_OR] = "|",
	[TOKEN_OR_EQ] = "|=",
	[TOKEN_PLUS] = "+",
	[TOKEN_PLUS_EQ] = "+=",
	[TOKEN_PP_NUMBER] = "PP_NUMBER",
	[TOKEN_QUESTION_MARK] = "?",
	[TOKEN_RBRACE] = "}",
	[TOKEN_RBRACKET] = "]",
	[TOKEN_REGISTER] = "register",
	[TOKEN_RESTRICT] = "restrict",
	[TOKEN_RETURN] = "return",
	[TOKEN_RPAREN] = ")",
	[TOKEN_SEMICOLON] = ";",
	[TOKEN_SHL] = "<<",
	[TOKEN_SHL_EQ] = "<<=",
	[TOKEN_SHORT] = "short",
	[TOKEN_SHR] = ">>",
	[TOKEN_SHR_EQ] = ">>=",
	[TOKEN_SIGNED] = "signed",
	[TOKEN_SIZEOF] = "sizeof",
	[TOKEN_STATIC] = "static",
	[TOKEN_STATIC_ASSERT] = "_Static_assert",
	[TOKEN_STRING_LITERAL] = "STRING_LITERAL",
	[TOKEN_STRUCT] = "struct",
	[TOKEN_SWITCH] = "switch",
	[TOKEN_THREAD_LOCAL] = "_Thread_local",
	[TOKEN_TYPEDEF] = "typedef",
	[TOKEN_UNION] = "union",
	[TOKEN_UNSIGNED] = "unsigned",
	[TOKEN_VOID] = "void",
	[TOKEN_VOLATILE] = "volatile",
	[TOKEN_WHILE] = "while",
	[TOKEN_XOR] = "^",
	[TOKEN_XOR_EQ] = "^=",
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


mcc_error_t lex_name(struct lexer *lexer, struct token_data *token)
{
	size_t i = 0;

	while (is_letter(*lexer->c) || is_digit(*lexer->c) || *lexer->c == '_' || *lexer->c == '\\') {
		token->ident[i++] = *lexer->c;
		lexer->c++;
	}

	token->ident[i] = '\0';

	struct symbol *symbol = symtab_search(lexer->symtab, token->ident);

	if (lexer->context == LEXER_CTX_C_SOURCE) {
		if (symbol && symbol->type == SYMBOL_C_KEYWORD)
			token->token = symbol->keyword;
		else
			token->token = TOKEN_IDENT;

		return MCC_ERROR_OK;
	}
	else if (lexer->context == LEXER_CTX_CPP_SOURCE) {
		if (symbol && symbol->type == SYMBOL_CPP_DIRECTIVE)
			token->token = symbol->keyword;
		else
			token->token = TOKEN_IDENT;

		return MCC_ERROR_OK;
	}

	token->token = TOKEN_IDENT;
	return MCC_ERROR_OK;
}


static mcc_error_t lex_char(struct lexer *lexer, struct token_data *token)
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


static inline bool lexer_is_eol(struct lexer *lexer)
{
	return (lexer->c - lexer->line) >= lexer->line_len;
}


mcc_error_t lex_pp_number(struct lexer *lexer, struct token_data *token)
{
	char c;
	size_t i = 0;

	while (!lexer_is_eol(lexer)) {
		if (*lexer->c == '+' || *lexer->c == '-') {
			assert(lexer->c != lexer->line);

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

		token->string[i++] = *lexer->c;
		lexer->c++;
	}

	token->token = TOKEN_PP_NUMBER;
	token->string[i] = '\0';

	return MCC_ERROR_OK;
}


mcc_error_t lex_string(struct lexer *lexer, struct token_data *token)
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

		assert(s != NULL); // TODO

		s->type = SYMBOL_C_KEYWORD;
		s->keyword = keywords[i];
	}

	for (i = 0; i < ARRAY_SIZE(cpp_directives); i++) {
		struct symbol *s = symtab_insert(lexer->symtab, token_names[cpp_directives[i]]);

		assert(s != NULL); // TODO

		s->type = SYMBOL_CPP_DIRECTIVE;
		s->keyword = cpp_directives[i];
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

	lexer->context = LEXER_CTX_C_SOURCE;

	return MCC_ERROR_OK;
}


void lexer_set_buffer(struct lexer *lexer, struct inbuf *buffer)
{
	assert(lexer->buffer == NULL);

	lexer->buffer = buffer;

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


/*
 * TODO Add trigraph support.
 */
static mcc_error_t lexer_read_line(struct lexer *lexer)
{
	int c;
	size_t i = 0;
	bool escape = false;

	assert(lexer->buffer != NULL);

	for (; (c = inbuf_get_char(lexer->buffer)) != INBUF_EOF; ) {
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

		if (lexer->context == LEXER_CTX_CPP_SOURCE) {
			token->token = TOKEN_EOL;
			return err;
		}

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
			return lex_string(lexer, token);
		}

	case 'U':
	case 'L':
		/* L'char' or U'char' or u'char' */
		if (*lexer->c == '\'') {
			lexer->c++;
			return lex_char(lexer, token);
		}
		
		/* L"string" or U"string" or u"string" */
		if (*lexer->c == '\"') {
			lexer->c++;
			return lex_string(lexer, token);
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
		return lex_name(lexer, token);

	case '\"':
		return lex_string(lexer, token);

	case '\'':
		return lex_char(lexer, token);

	case '.':
		if (!is_digit(*lexer->c)) {
			if (*lexer->c == '.' && lexer->c[1] == '.') {
				token->token = TOKEN_ELLIPSIS;
				lexer->c += 2;
			}
			else {
				token->token = TOKEN_DOT;
			}

			break;
		}

		/* fall-through to pp-number processing */ 

	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9':
		lexer->c--;
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


void lexer_dump_token(struct token_data *token)
{
	const char *name = token_names[token->token];
	char tmp[32];
	char c;
	size_t i = 0;

	switch (token->token) {
	case TOKEN_STRING_LITERAL:
		printf("\"");
		while ((c = token->string[i++])) {
			char_to_printable(c, tmp, sizeof(tmp));
			printf("%s", tmp);
		}
		printf("\"");
		break;

	case TOKEN_CHAR_CONST:
		char_to_printable(token->value, tmp, sizeof(tmp));
		printf("\'%s\'", tmp);
		break;

	case TOKEN_IDENT:
		printf("[%s]", token->ident);
		break;

	case TOKEN_PP_NUMBER:
		printf("%s", token->string);
		break;

	default:
		printf("%s", name);
	}

	putchar(' ');
}


void lexer_set_context(struct lexer *lexer, enum lexer_ctx context)
{
	lexer->context = context;
}
