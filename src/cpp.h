#ifndef CPP_H
#define CPP_H

#include "lexer.h"
#include "error.h"

struct cpp
{
	struct lexer lexer;
	struct inbuf inbuf;
	struct symtab *table;
};

mcc_error_t cpp_init(struct cpp *cpp);
mcc_error_t cpp_open(struct cpp *cpp, const char *filename);
void cpp_close(struct cpp *cpp);
void cpp_set_symtab(struct cpp *cpp, struct symtab *table);
mcc_error_t cpp_next(struct cpp *cpp, struct tokinfo *token);
void cpp_free(struct cpp *cpp);

#endif
