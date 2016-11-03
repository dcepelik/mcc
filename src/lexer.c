#include "debug.h"
#include "lexer.h"
#include <assert.h>


/*
 * Rule 1: Every function operating on the lexer shall alter the state in such
 *         way that lex->c points to first unprocessed character.
 */


static enum token token_lookup[256];


void lexer_reset(struct lexer *lexer, struct buffer *buffer)
{
	lexer->buffer = buffer;
	lexer->pushback = false;
}


static bool is_letter(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}


static bool is_digit(char c)
{
	return c >= '0' && c <= '9';
}


static bool is_whitespace(char c)
{
	return c == '\n' || c == '\r' || c == '\t' || c == 0x0C || c == ' ';
}


static bool is_hex_digit(char c)
{
	return is_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}


static bool is_octal_digit(char c)
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
			if (escape) { // unlikely
				escape = false;

				switch (c) {
				case 'n':
					token->string[i] = '\n';
					continue;

				case '"':
					token->string[i] = '\"';
					continue;

				case '\\':
					token->string[i] = '\\';
					continue;

				case 'x':
					/* hexadecimal-escape-sequence */
					while ((c = get_char(lexer)) != -1) {
						if (!is_hex_digit(c))
							break;

						// eat this hex digit
					}
					unget_char(lexer, c);
					token->string[i] = 'X';
					continue;

				case 'u':
					for (int l = 0; l < 4; l++) {
						c = get_char(lexer);
						if (!is_hex_digit(c)) {
							DEBUG_PRINTF("Error: not a hex digit: %c", c);
						}
						else {
							// eat this hex digit
						}
					}
					token->string[i] = 'u';
					continue;

				case 'U':
					DEBUG_MSG("Do case 'u' twice");
					continue;

				default:
					if (is_octal_digit(c)) {
						int j = 1;
						do {
							// eat this octal digit
							c = get_char(lexer);
							j++;
						} while (j <= 3 && is_octal_digit(c));

						unget_char(lexer, c);

						token->string[i] = '8';
						continue;
					}
					else {
						// error
						DEBUG_PRINTF("Invalid escape sequence: \\%c", c);
						continue;
					}
				}
			}

			if (c == '\\') {
				escape = true;
				i--;
				continue;
			}

			if (c == '\"') {
				break;
			}

			if (c == '\n') {
				DEBUG_MSG("Missing \"");
				break;
			}

			token->string[i] = c;
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
		i = 1;
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
		// do some simple table lookup
	}

	return false;
}


int read_hex_quad(struct lexer *lex)
{
}
