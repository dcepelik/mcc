#ifndef LEXER_H
#define LEXER_H

#define IDENT_MAX_LEN	64
#define INIT_LINE_SIZE	1024

#define	FLAG_PREFIX_L	1
#define FLAG_PREFIX_U	2
#define FLAG_PREFIX_u	4
#define FLAG_PREFIX_u8	8

#include "inbuf.h"
#include <stdbool.h>

enum lexer_eol {
	LEXER_EOL_MAC,
	LEXER_EOL_MSDOS,
	LEXER_EOL_UNIX,
	LEXER_EOL_UNKNOWN,
};

struct lexer {
	struct inbuf *buffer;	/* the input buffer being lexed */
	struct symtab *symtab;	/* symbol table */
	char *line;		/* current (logical) line being lexed */
	size_t line_len;	/* length of current line */
	size_t line_size;	/* length of memory block that line points to */
	enum lexer_eol eol;	/* EOL convention used by the file */
	char *c;		/* points to current character within line */
	size_t line_no;		/* (physical) line number */
	size_t col_no;		/* (physical) column number */
};

enum token {
	TOKEN_ALIGNAS, TOKEN_ALIGNOF, TOKEN_ATOMIC, TOKEN_BOOL, TOKEN_COMPLEX,
	TOKEN_GENERIC, TOKEN_IMAGINARY, TOKEN_NORETURN, TOKEN_STATIC_ASSERT,
	TOKEN_THREAD_LOCAL, TOKEN_AUTO, TOKEN_BREAK, TOKEN_CASE, TOKEN_CHAR,
	TOKEN_CONST, TOKEN_CONTINUE, TOKEN_DEFAULT, TOKEN_DO, TOKEN_DOUBLE,
	TOKEN_ELSE, TOKEN_ENUM, TOKEN_EXTERN, TOKEN_FLOAT, TOKEN_FOR,
	TOKEN_GOTO, TOKEN_IF, TOKEN_INLINE, TOKEN_INT, TOKEN_LONG,
	TOKEN_REGISTER, TOKEN_RESTRICT, TOKEN_RETURN, TOKEN_SHORT, TOKEN_SIGNED,
	TOKEN_SIZEOF, TOKEN_STATIC, TOKEN_STRUCT, TOKEN_SWITCH, TOKEN_TYPEDEF,
	TOKEN_UNION, TOKEN_UNSIGNED, TOKEN_VOID, TOKEN_VOLATILE, TOKEN_WHILE,

	TOKEN_HEADER_NAME, TOKEN_IDENT, TOKEN_PP_NUMBER, TOKEN_CHAR_CONST,
	TOKEN_STRING_LITERAL,

	TOKEN_LBRACKET, TOKEN_RBRACKET, TOKEN_LPAREN, TOKEN_RPAREN,
	TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_DOT, TOKEN_ARROW, TOKEN_INC,
	TOKEN_DEC, TOKEN_AMPERSAND, TOKEN_ASTERISK, TOKEN_PLUS, TOKEN_MINUS,
	TOKEN_NEG, TOKEN_NOT, TOKEN_DIV, TOKEN_MOD, TOKEN_SHL,
	TOKEN_SHR, TOKEN_LT, TOKEN_GT, TOKEN_LE, TOKEN_GE, TOKEN_EQEQ, TOKEN_NEQ,
	TOKEN_XOR, TOKEN_OR, TOKEN_LOGICAL_AND, TOKEN_LOGICAL_OR,
	TOKEN_QUESTION_MARK, TOKEN_COLON, TOKEN_SEMICOLON, TOKEN_ELLIPSIS,
	TOKEN_EQ, TOKEN_MUL_EQ, TOKEN_DIV_EQ, TOKEN_MOD_EQ, TOKEN_PLUS_EQ,
	TOKEN_MINUS_EQ, TOKEN_SHL_EQ, TOKEN_SHR_EQ, TOKEN_AND_EQ, TOKEN_XOR_EQ,
	TOKEN_OR_EQ, TOKEN_COMMA, TOKEN_HASH, TOKEN_HASH_HASH,

	TOKEN_EOF, TOKEN_ERROR
};

struct token_data {
	enum token token;

	union {
		char ident[IDENT_MAX_LEN];
		char string[IDENT_MAX_LEN];
		int value;
	};

	int prefix_flags;
};

mcc_error_t lexer_init(struct lexer *lexer);
void lexer_set_buffer(struct lexer *lexer, struct inbuf *buf);
void lexer_set_symtab(struct lexer *lexer, struct symtab *symtab);
mcc_error_t lexer_next(struct lexer *lexer, struct token_data *token_data);
void lexer_free(struct lexer *lexer);
void lexer_dump_token(struct token_data *token);

#endif
