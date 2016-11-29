#include "cpp.h"
#include "cppfile.h"
#include <stdlib.h>


struct cppfile *cppfile_new()
{
	struct cppfile *file = malloc(sizeof(*file));
	if (!file)
		return NULL;

	return file;
}	


mcc_error_t cppfile_open(struct cppfile *file, char *filename)
{
	mcc_error_t err;

	/* TODO Get rid of random init constants. */

	if (!strbuf_init(&file->linebuf, 1024))
		return MCC_ERROR_NOMEM;

	if (!strbuf_init(&file->strbuf, 1024)) {
		strbuf_free(&file->linebuf);
		return MCC_ERROR_NOMEM;
	}

	if ((err = inbuf_open(&file->inbuf, 1024, filename)) != MCC_ERROR_OK)
		goto out_err;

	file->c = strbuf_get_string(&file->linebuf);
	file->inside_include = false;
	file->next_at_bol = true;
	file->first_token = true;

	file->cur = NULL;
	
	mempool_init(&file->token_data, 2048);
	objpool_init(&file->tokinfo_pool, sizeof(struct tokinfo), 256);
	list_init(&file->tokens);

	return MCC_ERROR_OK;

out_err:
	strbuf_free(&file->linebuf);
	strbuf_free(&file->strbuf);

	return err;
}


void cppfile_close(struct cppfile *file)
{
	strbuf_free(&file->linebuf);
	strbuf_free(&file->strbuf);
	inbuf_close(&file->inbuf);
	mempool_free(&file->token_data);
	objpool_free(&file->tokinfo_pool);
	list_free(&file->tokens);
}


void cppfile_delete(struct cppfile *file)
{
	free(file);
}


void cppfile_set_symtab(struct cppfile *file, struct symtab *table)
{
	file->symtab = table;
	cpp_setup_symtab(file);
}
