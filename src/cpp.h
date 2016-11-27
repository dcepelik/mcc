#ifndef CPP_H
#define CPP_H

#include "lexer.h"
#include "error.h"
#include "list.h"

struct cpp
{
	struct lexer lexer;		/* lexer */
	struct inbuf inbuf;		/* input buffer */
	struct symtab *table;		/* symbol table */
	struct list tokens;		/* list of tokens */
	struct tokinfo *cur;		/* last popped tokinfo */
	struct list ifs;		/* #if directive stack */
};


enum cpp_directive
{
	CPP_DIRECTIVE_IF,
	CPP_DIRECTIVE_IFDEF,
	CPP_DIRECTIVE_IFNDEF,
	CPP_DIRECTIVE_ELIF,
	CPP_DIRECTIVE_ELSE,
	CPP_DIRECTIVE_ENDIF,
	CPP_DIRECTIVE_INCLUDE,
	CPP_DIRECTIVE_DEFINE,
	CPP_DIRECTIVE_UNDEF,
	CPP_DIRECTIVE_LINE,
	CPP_DIRECTIVE_ERROR,
	CPP_DIRECTIVE_PRAGMA,
};

mcc_error_t cpp_init(struct cpp *cpp);
mcc_error_t cpp_open(struct cpp *cpp, const char *filename);
void cpp_close(struct cpp *cpp);
void cpp_set_symtab(struct cpp *cpp, struct symtab *table);
struct tokinfo *cpp_next(struct cpp *cpp);
void cpp_free(struct cpp *cpp);

#endif
