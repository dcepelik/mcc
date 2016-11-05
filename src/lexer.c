#include "debug.h"
#include "lexer.h"
#include <inttypes.h>
#include <assert.h>
	

static const char simple_escape_sequence[256] = {
	['\''] = '\'', ['\"'] = '\"', ['\?'] = '\?', ['\\'] = '\\',
	['a'] = '\a', ['b'] = '\b', ['f'] = '\f', ['n'] = '\n',
	['r'] = '\r', ['t'] = '\t', ['v'] = '\v',
};


void lexer_reset(struct lexer *lexer, struct buffer *buffer)
{
	lexer->buffer = buffer;
	lexer->pushback = false;
}


inline static bool is_letter(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
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


static int get_char(struct lexer *lexer)
{
	if (!lexer->pushback) {
		return buffer_get_char(lexer->buffer);
	}

	lexer->pushback = false;
	return lexer->pushback_char;
}


static void unget_char(struct lexer *lexer, char c)
{
	assert(!lexer->pushback);

	lexer->pushback = true;
	lexer->pushback_char = c;
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


static uint32_t read_octal_number(struct lexer *lexer)
{
	uint32_t val = 0;
	int c;
	int i;

	for (i = 0; i < 3; i++) {
		c = get_char(lexer);

		if (!is_octal_digit(c)) {
			unget_char(lexer, c);
			break;
		}

		val *= 8;
		val += c - '0';
	}

	return val;
}


static uint32_t read_hex_number(struct lexer *lexer, size_t min_len, size_t max_len)
{	
	uint32_t val = 0;
	int c;
	int i;

	for (i = 0; i < max_len; i++) {
		c = get_char(lexer);

		if (!is_hex_digit(c)) {
			if (i < min_len)
				DEBUG_PRINTF("error: character %c was unexpected, expected a hex digit", c);

			unget_char(lexer, c);
			break;
		}

		val *= 16;
		val += hex_val(c);
	}

	return val;
}


static uint32_t read_escape_sequence(struct lexer *lexer)
{
	int c = get_char(lexer);

	if (c > 0 && simple_escape_sequence[c])
		return simple_escape_sequence[c];

	if (is_octal_digit(c)) {
		unget_char(lexer, c);
		return read_octal_number(lexer);
	}

	switch (c) {
	case 'x':	
		return read_hex_number(lexer, 1, 8);

	case 'u':
		return read_hex_number(lexer, 4, 4);

	case 'U':
		return read_hex_number(lexer, 8, 8);

	default:
		DEBUG_PRINTF("error: unknown escape sequence: \\%c", c);
	}

	return -1;
}


bool lexer_next_token(struct lexer *lexer, struct token_data *token)
{
	bool escape = false;
	int i;
	int c;

again:
	c = get_char(lexer);

	/*
	 * identifier (possibly beginning with a universal-character-name)
	 * L"string", U"string", u"string" or u8"string"
	 * L'char', U'string', u'char'
	 */
	if (is_letter(c) || c == '_' || c == '\\') {
		char *str = token->ident;

		int i = 0;
		do {
			/* TODO handle universal character names */

			str[i++] = c;
			c = get_char(lexer);
		}
		while (is_letter(c) || is_digit(c) || c == '_' || c == '\\');

		unget_char(lexer, c);
		str[i] = '\0';

		if (i <= 2 && (c == '\'' || c == '\"')) { /* unlikely */
			token->prefix_flags = 0;

			if (i == 1) {
				switch (str[0]) {
				case 'L':
					token->prefix_flags = FLAG_PREFIX_L;
					break;

				case 'U':
					token->prefix_flags = FLAG_PREFIX_U;
					break;

				case 'u':
					token->prefix_flags = FLAG_PREFIX_u;
					break;
				}
			}
			else if (c == '\"' && str[0] == 'u' && str[1] == '8') {
				token->prefix_flags = FLAG_PREFIX_u8;
			}
			
			if (token->prefix_flags) {
				if (c == '\'') {
					goto char_const;
				}
				else {
					goto string_literal;
				}
			}
		}

		/* check whether this is a keyword */

		token->token = TOKEN_IDENT;
		return true;
	}
	else if (is_whitespace(c)) {
		goto again;
	}
	/*
	 * string literal
	 */
	else if (c == '\"') {
string_literal:
		token->token = TOKEN_STRING_LITERAL;

		for (i = 0; (c = get_char(lexer)) != -1; i++) {
			if (c == '\"')
				break;

			if (c == '\\') {
				token->string[i] = read_escape_sequence(lexer);
			}
			else if (c == '\n') {
				DEBUG_MSG("error: missing the final \"");
				break;
			}
			else {
				token->string[i] = c;
			}
		}
		token->string[i] = '\0';

		return true;
	}
	/*
	 * pp-number
	 * TOKEN_DOT
	 * TOKEN_ELLIPSIS
	 */
	else if (is_digit(c) || c == '.') {
		
	}
	/*
	 * char-const
	 */
	else if (c == '\'') {
char_const:
		token->token = TOKEN_CHAR_CONST;

		uint32_t val;
		for (i = 0; (c = get_char(lexer)) != -1; i++) {
			if (c == '\'') {
				if (i == 0)
					DEBUG_MSG("error: empty character constant");

				break;
			}

			if (c == '\\') {
				val = read_escape_sequence(lexer);
			}
			else if (c == '\n') {
				DEBUG_MSG("error: missing the final \"");
				break;
			}
			else {
				val = (int)c;
			}
		}

		token->value = val;

		return true;
	}
	/*
	 * line comment
	 * block comment
	 * TOKEN_SOLIDUS
	 */
	else if (c == '/') {
	}
	/*
	 * punctuator
	 */
	else {
		switch (c) {
		case '{':
			token->token = TOKEN_LBRACE;
			break;

		case '}':
			token->token = TOKEN_RBRACE;
			break;

		case '(':
			token->token = TOKEN_LPAREN;
			break;

		case ')':
			token->token = TOKEN_RPAREN;
			break;

		case ';':
			token->token = TOKEN_SEMICOLON;
			break;

		default:
			DEBUG_PRINTF("error: unexpected %c", c);
			return false;
		}

		return true;

	}

	return false;
}
