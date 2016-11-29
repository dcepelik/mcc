#include "cpp.h"
#include "cppfile.h"
#include "strbuf.h"
#include <stdarg.h>
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

	file->line_no = 0;
	
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


void cppfile_error(struct cppfile *file, char *fmt, ...)
{
	struct strbuf buf;
	char *line;
	size_t i;

	(void) file;

	line = strbuf_get_string(&file->linebuf);

	va_list args;
	va_start(args, fmt);
	strbuf_init(&buf, 1024);

	strbuf_printf(&buf, "error: ");
	strbuf_vprintf_at(&buf, strbuf_strlen(&buf), fmt, args);
	strbuf_printf(&buf, "\n%s\n", line);

	for (i = 0; i < file->column_no; i++)
		if (line[i] == '\t')
			strbuf_putc(&buf, '\t');
		else
			strbuf_putc(&buf, ' ');

	strbuf_printf(&buf, "^^^");

	fprintf(stderr, "%s\n", strbuf_get_string(&buf));

	va_end(args);
	strbuf_free(&buf);
}
