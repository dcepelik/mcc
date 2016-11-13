#include "cpp.h"


mcc_error_t cpp_init(struct cpp *cpp)
{
	return lexer_init(&cpp->lexer);
}


void cpp_free(struct cpp *cpp)
{
	lexer_free(&cpp->lexer);
}


mcc_error_t cpp_open(struct cpp *cpp, const char *filename)
{
	mcc_error_t err;

	err = inbuf_open(&cpp->inbuf, 1024, filename);
	if (err != MCC_ERROR_OK)
		return err;

	lexer_set_inbuf(&cpp->lexer, &cpp->inbuf);

	return MCC_ERROR_OK;
}


void cpp_close(struct cpp *cpp)
{
	inbuf_close(&cpp->inbuf);
}


void cpp_set_symtab(struct cpp *cpp, struct symtab *table)
{
	cpp->table = table;
	lexer_set_symtab(&cpp->lexer, table);
}


mcc_error_t cpp_next(struct cpp *cpp, struct tokinfo *tokinfo)
{
	mcc_error_t err;

	err = lexer_next(&cpp->lexer, tokinfo);
	if (err != MCC_ERROR_OK)
		return err;

	switch (tokinfo->token)
	{
	case TOKEN_HASH:
		break;

	default:
		break;
	}

	return err;
}
