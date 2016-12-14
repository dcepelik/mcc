#ifndef CPP_H
#define CPP_H

#include "error.h"
#include "errlist.h"
#include "lexer.h"
#include "list.h"
#include "mempool.h"
#include "objpool.h"
#include "token.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * C preprocessor state. This structure owns all token-related memory
 * and maintains the stack of all open input files.
 */
struct cpp
{
	struct symtab *symtab;		/* symbol table */
	struct objpool token_pool;	/* objpool for struct token */
	struct mempool token_data;	/* mempool for misc token data */
	struct objpool macro_pool;	/* objpool for macros */
	struct objpool file_pool;	/* objpool for open files */

	struct list file_stack;		/* stack of open files */
	struct list tokens;		/* token queue */
	struct token *token;		/* last popped token */
	struct list ifs;		/* if directive control stack */

	struct errlist errlist;		/* error list */
};

struct cpp *cpp_new();
void cpp_delete(struct cpp *cpp);

mcc_error_t cpp_open_file(struct cpp *cpp, char *filename);
void cpp_close_file(struct cpp *file);

struct token *cpp_next(struct cpp *cpp);

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

void cpp_dump_toklist(struct list *lst, FILE *fout);
void cpp_dump_file_stack(struct cpp *cpp);

#endif
