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
	struct tokinfo *current;	/* current token information */
};

mcc_error_t cpp_init(struct cpp *cpp);
mcc_error_t cpp_open(struct cpp *cpp, const char *filename);
void cpp_close(struct cpp *cpp);
void cpp_set_symtab(struct cpp *cpp, struct symtab *table);
struct tokinfo *cpp_next(struct cpp *cpp);
void cpp_free(struct cpp *cpp);

#endif
