#ifndef CPP_H
#define CPP_H

#include "lexer.h"
#include "error.h"

struct cpp
{
	struct lexer lexer;		/* lexer */
	struct inbuf inbuf;		/* input buffer */
	struct symtab *table;		/* symbol table */
	struct tokinfo **tokens;	/* token buffer */
	struct tokinfo **cur;		/* current token in tokens */
};

mcc_error_t cpp_init(struct cpp *cpp);
mcc_error_t cpp_open(struct cpp *cpp, const char *filename);
void cpp_close(struct cpp *cpp);
void cpp_set_symtab(struct cpp *cpp, struct symtab *table);
struct tokinfo *cpp_next(struct cpp *cpp);
void cpp_free(struct cpp *cpp);

#endif
