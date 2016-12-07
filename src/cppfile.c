#include "cpp.h"
#include "cppfile.h"
#include "debug.h"
#include "macro.h"
#include "strbuf.h"
#include <stdarg.h>
#include <stdlib.h>

#define STRBUF_INIT_SIZE	128
#define INBUF_BLOCK_SIZE	2048
#define TOKEN_POOL_BLOCK_SIZE	256
#define MACRO_POOL_BLOCK_SIZE	32
#define TOKEN_DATA_BLOCK_SIZE	1024


struct cppfile *cppfile_new()
{
	return malloc(sizeof(struct cppfile));
}	


mcc_error_t cppfile_open(struct cppfile *file, char *filename)
{
	mcc_error_t err;

	/* TODO Get rid of random init constants. */

	if (!strbuf_init(&file->linebuf, STRBUF_INIT_SIZE))
		return MCC_ERROR_NOMEM;

	if (!strbuf_init(&file->strbuf, STRBUF_INIT_SIZE)) {
		strbuf_free(&file->linebuf);
		return MCC_ERROR_NOMEM;
	}

	if ((err = inbuf_open(&file->inbuf, INBUF_BLOCK_SIZE, filename)) != MCC_ERROR_OK)
		goto out_err;

	file->c = strbuf_get_string(&file->linebuf);

	mempool_init(&file->token_data, TOKEN_DATA_BLOCK_SIZE);
	objpool_init(&file->token_pool, sizeof(struct token), TOKEN_POOL_BLOCK_SIZE);
	objpool_init(&file->macro_pool, sizeof(struct macro), MACRO_POOL_BLOCK_SIZE);

	file->inside_include = false;
	file->next_at_bol = true;
	file->first_token = true;
	file->had_whitespace = false;

	file->location.line_no = 0;
	
	list_init(&file->tokens);
	file->token = NULL;
	list_init(&file->ifs);
	file->included_file = NULL;

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
	objpool_free(&file->token_pool);
	objpool_free(&file->macro_pool);
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
	struct location loc;

	(void) file;

	line = strbuf_get_string(&file->linebuf);
	loc = file->token->startloc;

	va_list args;
	va_start(args, fmt);
	strbuf_init(&buf, 1024);

	strbuf_printf(&buf, "error: ");
	strbuf_vprintf_at(&buf, strbuf_strlen(&buf), fmt, args);
	strbuf_printf(&buf, "\n%s\n", line);

	for (i = 0; i < loc.column_no; i++) {
		if (line[i] == '\t')
			strbuf_putc(&buf, '\t');
		else
			strbuf_putc(&buf, ' ');
	}

	strbuf_printf(&buf, "^^^");

	fprintf(stderr, "%s\n", strbuf_get_string(&buf));

	va_end(args);
	strbuf_free(&buf);
}
