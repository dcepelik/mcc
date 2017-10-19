#include "common.h"
#include "errlist.h"
#include <assert.h>
#include <stdarg.h>

#define STRING_POOL_BLOCK_SIZE	512
#define ERROR_POOL_BLOCK_SIZE	16

void errlist_init(struct errlist *errlist)
{
	size_t i;

	list_init(&errlist->errors);
	mempool_init(&errlist->string_pool, STRING_POOL_BLOCK_SIZE);
	objpool_init(&errlist->error_pool, sizeof(struct error), ERROR_POOL_BLOCK_SIZE);

	for (i = 0; i < ARRAY_SIZE(errlist->num_errors_by_level); i++)
		errlist->num_errors_by_level[i] = 0;
}

void errlist_free(struct errlist *errlist)
{
	list_free(&errlist->errors);
	mempool_free(&errlist->string_pool);
	objpool_free(&errlist->error_pool);
}

void errlist_insert(struct errlist *errlist, enum error_level level,
	char *filename, char *message, char *context, size_t context_len,
	struct location location)
{
	struct error *error = objpool_alloc(&errlist->error_pool);
	error->level = level;
	error->filename = mempool_strdup(&errlist->string_pool, filename);
	error->message = mempool_strdup(&errlist->string_pool, message);
	error->context = mempool_memcpy(&errlist->string_pool, context, context_len);
	error->location = location;

	assert(error->level < ARRAY_SIZE(errlist->num_errors_by_level));
	errlist->num_errors_by_level[error->level]++;

	list_insert(&errlist->errors, &error->list_node);
}

const char *error_level_to_string(enum error_level level)
{
	switch (level) {
	case ERROR_LEVEL_NOTICE:
		return "notice";

	case ERROR_LEVEL_WARNING:
		return "warning";

	case ERROR_LEVEL_ERROR:
		return "error";

	case ERROR_LEVEL_FATAL:
		return "fatal error";
	}

	return NULL;
}

static void error_dump(struct error *error, FILE *fout)
{
	size_t i;

	fprintf(fout, "%s: %lu: %s: %s\n", error->filename,
		error->location.line_no, error_level_to_string(error->level),
		error->message);

	if (error->context) {
		fprintf(fout, "%s\n", error->context);

		/* print the problem-mark */
		for (i = 0; i < error->location.column_no; i++) {
			if (error->context[i] == '\t')
				fputc('\t', fout);
			else
				fputc(' ', fout);
		}
		fputc('^', fout);
		fputc('\n', fout);
	}
}

void errlist_dump(struct errlist *errlist, FILE *fout)
{
	list_foreach(struct error, error, &errlist->errors, list_node) {
		error_dump(error, fout);

		if (error != list_last(&errlist->errors))
			fputc('\n', fout);
	}
}
