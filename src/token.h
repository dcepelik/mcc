#ifndef TOKINFO_H
#define TOKINFO_H

#include "list.h"
#include "strbuf.h"
#include <stdbool.h>
#include <stdlib.h>

enum token_type
{
	/* pp-token categories (except punctuators) */
	TOKEN_CHAR_CONST, TOKEN_HEADER_NAME, TOKEN_NAME, TOKEN_NUMBER,
	TOKEN_STRING,

	/* punctuators */
	TOKEN_AMPERSAND, TOKEN_AND_EQ, TOKEN_ARROW, TOKEN_ASTERISK, TOKEN_COLON,
	TOKEN_COMMA, TOKEN_DEC, TOKEN_DIV, TOKEN_DIV_EQ, TOKEN_DOT,
	TOKEN_ELLIPSIS, TOKEN_EQ, TOKEN_EQ_EQ, TOKEN_GE, TOKEN_GT, TOKEN_HASH,
	TOKEN_HASH_HASH, TOKEN_INC, TOKEN_LBRACE, TOKEN_LBRACKET, TOKEN_LE,
	TOKEN_LOGICAL_AND, TOKEN_LOGICAL_OR, TOKEN_LPAREN, TOKEN_LT,
	TOKEN_MINUS, TOKEN_MINUS_EQ, TOKEN_MOD, TOKEN_MOD_EQ, TOKEN_MUL_EQ,
	TOKEN_NEG, TOKEN_NEQ, TOKEN_NOT, TOKEN_OR, TOKEN_OR_EQ, TOKEN_PLUS,
	TOKEN_PLUS_EQ, TOKEN_QUESTION_MARK, TOKEN_RBRACE, TOKEN_RBRACKET,
	TOKEN_RPAREN, TOKEN_SEMICOLON, TOKEN_SHL, TOKEN_SHL_EQ, TOKEN_SHR,
	TOKEN_SHR_EQ, TOKEN_XOR, TOKEN_XOR_EQ,

	TOKEN_EOF, TOKEN_EOL
};

struct location
{
	size_t line_no;
	size_t column_no;
};

struct token
{
	struct list_node list_node;

	union
	{
		struct symbol *symbol;
		char *str;
		int value;
		int c;
	};

	struct location startloc;
	struct location endloc;

	enum token_type type;

	/* flags */
	bool preceded_by_whitespace;
	bool is_at_bol;
	bool noexpand;		/* don't expand this token even if it's macro */
};

const char *token_get_name(enum token_type token);
void token_print(struct token *token, struct strbuf *buf);
void token_dump(struct token *token);

#endif
