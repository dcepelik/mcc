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
