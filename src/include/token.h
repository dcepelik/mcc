#ifndef TOKEN_H
#define TOKEN_H

#include "keyword.h"
#include "list.h"
#include "lstr.h"
#include "strbuf.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * C token.
 *
 * See 6.4 Lexical categories
 * See 6.4.6 Punctuators
 */
enum token_type
{
	/*
	 * The TOKEN_OP_* constants always translate to the
	 * OPER_* operator in `enum oper', regardless of context.
	 * For example, the `++' token always represents the
	 * unary incrementation operator.
	 *
	 * Values less than TOKEN_OP_CAST_MAX are safe to cast
	 * to `enum oper'.
	 *
	 * NOTE: Do not modify the order of items within this
	 *       block without modifying `enum oper' correspondingly.
	 */
	TOKEN_OP_ADDEQ,		/* OPER_ADDEQ */
	TOKEN_OP_DOT,		/* OPER_DOT */
	TOKEN_OP_AND,		/* OPER_AND,	*/
	TOKEN_OP_ARROW,		/* OPER_ARROW */
	TOKEN_OP_ASSIGN,	/* OPER_ASSIGN */
	TOKEN_OP_BITANDEQ,	/* OPER_BITAND */
	TOKEN_OP_BITOR,		/* OPER_BITOR */
	TOKEN_OP_BITOREQ,	/* OPER_BITOR */
	TOKEN_OP_DIV,		/* OPER_DIV */
	TOKEN_OP_DIVEQ,		/* OPER_DIVEQ */
	TOKEN_OP_EQ,		/* OPER_EQ */
	TOKEN_OP_GE,		/* OPER_GE */
	TOKEN_OP_GT,		/* OPER_GT */
	TOKEN_OP_LE,		/* OPER_LE */
	TOKEN_OP_LT,		/* OPER_LT */
	TOKEN_OP_MOD,		/* OPER_MOD */
	TOKEN_OP_MODEQ,		/* OPER_MODEQ */
	TOKEN_OP_MULEQ,		/* OPER_MULEQ */
	TOKEN_OP_NEG,		/* OPER_NEG */
	TOKEN_OP_NEQ,		/* OPER_NEQ */
	TOKEN_OP_NOT,		/* OPER_NOT */
	TOKEN_OP_OR,		/* OPER_OR */
	TOKEN_OP_SHL,		/* OPER_SHL */
	TOKEN_OP_SHLEQ,		/* OPER_SHLEQ */
	TOKEN_OP_SHR,		/* OPER_SHR */
	TOKEN_OP_SHREQ,		/* OPER_SHREQ */
	TOKEN_OP_SUBEQ,		/* OPER_SUBEQ */
	TOKEN_OP_XOR,		/* OPER_XOR */
	TOKEN_OP_XOREQ,		/* OPER_XOREQ */
	TOKEN_OP_CAST_MAX,	/* special value */

	/*
	 * Remaining punctuators. Some of these tokens will also
	 * translate to operators, but the translation is context-
	 * sensitive.
	 *
	 * For example, the TOKEN_AMP token may either translate
	 * to OPER_ADDROF or OPER_BITAND operators, depending on
	 * context in which it is used.
	 */
	TOKEN_AMP, TOKEN_ASTERISK, TOKEN_COLON, TOKEN_COMMA, TOKEN_ELLIPSIS,
	TOKEN_HASH, TOKEN_HASH_HASH, TOKEN_LBRACE, TOKEN_LBRACKET, TOKEN_LPAREN,
	TOKEN_MINUS, TOKEN_PLUS, TOKEN_QMARK, TOKEN_RBRACE, TOKEN_RBRACKET, TOKEN_RPAREN,
	TOKEN_SEMICOLON, TOKEN_MINUSMINUS, TOKEN_PLUSPLUS,	

	/*
	 * 6.4 Lexical categories
	 */
	TOKEN_CHAR_CONST, TOKEN_HEADER_HNAME, TOKEN_HEADER_QNAME, TOKEN_NAME,
	TOKEN_NUMBER, TOKEN_STRING_LITERAL, TOKEN_OTHER,

	/*
	 * Markers for internal use.
	 */
	TOKEN_EOF, TOKEN_EOL, TOKEN_PLACEMARKER
};

struct location
{
	char *filename;
	size_t line_no;
	size_t column_no;
};

void location_dump(struct location *loc);

/*
 * See 6.4.5 String literals syntax (encoding-prefix).
 */
enum enc_prefix
{
	ENC_PREFIX_NONE,	/* "string" or 'char' */
	ENC_PREFIX_L,		/* L"string" or L'char' */
	ENC_PREFIX_U,		/* u"string" or u'char' */
	ENC_PREFIX_UPPER_U,	/* U"string" or U'char' */
	ENC_PREFIX_U8,		/* u8"string */
};

struct token
{
	struct list_node list_node;	/* node for token lists */
	char *spelling;			/* token as given in input */

	enum token_type type;		/* token type */

	union
	{
		struct symbol *symbol;	/* for name tokens */
		char *str;		/* for pp-numbers, header names */
		struct lstr lstr;	/* for string literals */
		int value;		/* for number and char const tokens */
	};

	struct location startloc;	/* location where the token begins */
	struct location endloc;		/* location where the token ends */

	/* flags */
	bool after_white:1;		/* preceded by whitespace? */
	bool is_at_bol:1;		/* is at beginning of line? */
	bool noexpand:1;		/* don't expand this token */
	int enc_prefix:4;		/* encoding prefix */
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
bool token_is_keyword(struct token *token, enum kwd type);
bool token_is_any_keyword(struct token *token);
bool token_is_tqual(struct token *token);

char *token_to_string(struct token *token);
void token_print(struct token *token, struct strbuf *buf);
void token_dump(struct token *token, FILE *fout);

#endif
