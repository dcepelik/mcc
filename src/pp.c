#include "pp.h"


mcc_error_t pp_init(struct pp *pp)
{
	return lexer_init(&pp->lexer);
}


void pp_free(struct pp *pp)
{
	lexer_free(&pp->lexer);
}


void pp_set_buffer(struct pp *pp, struct inbuf *buffer)
{
	pp->buffer = buffer;
	lexer_set_buffer(&pp->lexer, pp->buffer);
}


void pp_set_symtab(struct pp *pp, struct symtab *table)
{
	pp->table = table;
	lexer_set_symtab(&pp->lexer, table);
}


void pp_next(struct pp *pp, struct token_data *token)
{
	(void)lexer_next(&pp->lexer, token);
}
