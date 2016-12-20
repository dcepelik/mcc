#ifndef CPP_INTERNAL_H
#define CPP_INTERNAL_H

#include "common.h"
#include "debug.h"
#include "error.h"
#include "lexer.h"
#include "mempool.h"
#include "objpool.h"
#include "token.h"
#include "toklist.h"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

/*
 * C preprocessor state.
 */
struct cpp
{
	struct context *ctx;

	struct symtab *symtab;		/* symbol table */
	struct mempool token_data;	/* mempool for misc token data */
	struct objpool macro_pool;	/* objpool for macros */
	struct objpool file_pool;	/* objpool for open files */

	struct list file_stack;		/* stack of open files */
	struct toklist tokens;		/* token queue */
	struct token *token;		/* last popped token */
	struct list ifs;		/* if directive control stack */
};

void cpp_dump_toklist(struct list *lst, FILE *fout);
void cpp_dump_file_stack(struct cpp *cpp);

void cpp_error(struct cpp *cpp, char *fmt, ...);
void cpp_warn(struct cpp *cpp, char *fmt, ...);

void cpp_next_token(struct cpp *cpp);
bool cpp_skipping(struct cpp *cpp);

struct cpp_file
{
	struct list_node list_node;
	char *filename;
	struct lexer lexer;
	struct inbuf inbuf;
};

mcc_error_t cpp_file_init(struct cpp *cpp, struct cpp_file *file, char *filename);

mcc_error_t cpp_open_file(struct cpp *cpp, char *filename);
void cpp_close_file(struct cpp *cpp);
void cpp_file_include(struct cpp *cpp, struct cpp_file *file);

struct cpp_file *cpp_cur_file(struct cpp *cpp);

/*
 * C preprocessor directives.
 */
enum cpp_directive
{
	CPP_DIRECTIVE_DEFINE,
	CPP_DIRECTIVE_ELIF,
	CPP_DIRECTIVE_ELSE,
	CPP_DIRECTIVE_ENDIF,
	CPP_DIRECTIVE_ERROR,
	CPP_DIRECTIVE_IF,
	CPP_DIRECTIVE_IFDEF,
	CPP_DIRECTIVE_IFNDEF,
	CPP_DIRECTIVE_INCLUDE,
	CPP_DIRECTIVE_LINE,
	CPP_DIRECTIVE_PRAGMA,
	CPP_DIRECTIVE_UNDEF,
};

void cpp_parse_directive(struct cpp *cpp);


struct cpp_if
{
	struct list_node list_node;
	struct token *token;
	bool skip_this_branch;
	bool skip_next_branch;
};

void cpp_init_ifstack(struct cpp *cpp);


/*
 * C preprocessor macro type.
 */
enum macro_type
{
	MACRO_TYPE_OBJLIKE,	/* an object-like macro */
	MACRO_TYPE_FUNCLIKE,	/* a function-like macro */
};

/*
 * C preprocessor macro.
 */
struct macro
{
	char *name;			/* name of the macro */
	struct toklist args;		/* argument list */
	struct toklist expansion;	/* expansion list */
	enum macro_type type;		/* type of the macro */
	bool is_expanding;		/* is this macro being expanded? */
};

void macro_init(struct macro *macro);
void macro_free(struct macro *macro);

bool macro_is_funclike(struct macro *macro);

void macro_dump(struct macro *macro);

void macro_expand(struct cpp *file, struct toklist *invocation, struct toklist *expansion);

/*
 * C preprocessor macro argument.
 */
struct macro_arg
{
	struct toklist tokens;		/* tokens that constitute this argument */
	struct toklist expansion;	/* complete macro expansion of @tokens */
};

#endif
