#include "pp.h"


mcc_error_t pp_init(struct pp *pp)
{
	return lexer_init(&pp->lexer);
}


void pp_free(struct pp *pp)
{
	lexer_free(&pp->lexer);
}


mcc_error_t pp_open(struct pp *pp, const char *filename)
{
	mcc_error_t err;

	err = inbuf_open(&pp->buffer, 1024, filename);
	if (err != MCC_ERROR_OK)
		return err;

	lexer_set_buffer(&pp->lexer, &pp->buffer);

	return MCC_ERROR_OK;
}


void pp_close(struct pp *pp)
{
	inbuf_close(&pp->buffer);
}


void pp_set_symtab(struct pp *pp, struct symtab *table)
{
	pp->table = table;
	lexer_set_symtab(&pp->lexer, table);
}


mcc_error_t pp_next(struct pp *pp, struct token_data *token)
{
	mcc_error_t err;

	err = lexer_next(&pp->lexer, token);
	if (err != MCC_ERROR_OK)
		return err;

	switch (token->token)
	{
	case TOKEN_HASH:
		lexer_set_context(&pp->lexer, LEXER_CTX_C_SOURCE);
		break;

	case TOKEN_EOL:
		lexer_set_context(&pp->lexer, LEXER_CTX_C_SOURCE);
		break;

	default:
		break;
	}

	return err;
}
