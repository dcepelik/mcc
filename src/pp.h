#ifndef PP_H
#define PP_H

#include "lexer.h"
#include "error.h"

struct pp
{
	struct lexer lexer;
	struct inbuf buffer;
	struct symtab *table;
};

mcc_error_t pp_init(struct pp *pp);
mcc_error_t pp_open(struct pp *pp, const char *filename);
void pp_close(struct pp *pp);
void pp_set_symtab(struct pp *pp, struct symtab *table);
mcc_error_t pp_next(struct pp *pp, struct token_data *token);
void pp_free(struct pp *pp);

#endif
