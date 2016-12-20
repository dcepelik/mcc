#ifndef TOKINFO_H
#define TOKINFO_H

#include "list.h"
#include "strbuf.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

enum token_type
{
	/* pp-token categories (except punctuators) */
	TOKEN_CHAR_CONST, TOKEN_HEADER_HNAME, TOKEN_HEADER_QNAME, TOKEN_NAME,
	TOKEN_NUMBER, TOKEN_STRING, TOKEN_OTHER, TOKEN_PLACEMARKER,

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

	/* internal markers */
	TOKEN_EOF, TOKEN_EOL
};

struct location
{
	char *filename;
	size_t line_no;
	size_t column_no;
};

void location_dump(struct location *loc);

struct token
{
	struct list_node list_node;
	char *spelling;			/* input that constitutes the token */

	union
	{
		struct symbol *symbol;	/* for name tokens */
		char *str;		/* for string tokens */
		int value;		/* for number tokens */
		int c;			/* for char const tokens */
	};

	struct location startloc;	/* beginning of the token */
	struct location endloc;		/* end of the token */

	enum token_type type;		/* token type */

	/* flags */
	bool preceded_by_whitespace;
	bool is_at_bol;
	bool noexpand;			/* don't expand this token */
};

const char *token_get_name(enum token_type token);
char *token_get_spelling(struct token *token);

bool token_is(struct token *token, enum token_type token_type);
bool token_is_name(struct token *token);
bool token_is_macro(struct token *token);
bool token_is_macro_arg(struct token *token);
bool token_is_eof(struct token *token);
bool token_is_eol(struct token *token);
bool token_is_eol_or_eof(struct token *token);

void token_print(struct token *token, struct strbuf *buf);
void token_dump(struct token *token, FILE *fout);

#endif
