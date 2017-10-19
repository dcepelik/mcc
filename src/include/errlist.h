/*
 * Error list manages the list of errors itself and error data allocation.
 */

#ifndef ERRLIST_H
#define ERRLIST_H

#include "list.h"
#include "mempool.h"
#include "objpool.h"
#include "token.h"
#include <stdio.h>

struct errlist
{
	struct list errors;		/* list of errors */
	size_t num_errors_by_level[4];	/* number of errors with given level */
	struct mempool string_pool;	/* mempool for strings */
	struct objpool error_pool;	/* objpool for errors */
};

void errlist_init(struct errlist *errlist);
void errlist_free(struct errlist *errlist);

void errlist_dump(struct errlist *errlist, FILE *fout);

enum error_level
{
	ERROR_LEVEL_NOTICE,	/* something worth noticing */
	ERROR_LEVEL_WARNING,	/* possible error */
	ERROR_LEVEL_ERROR,	/* regular error  */
	ERROR_LEVEL_FATAL	/* unrecoverable error */
};

const char *error_level_to_string(enum error_level level);

struct error
{
	struct lnode list_node;
	enum error_level level;
	char *filename;
	char *message;
	char *context;
	struct location location;
};

void errlist_insert(struct errlist *errlist, enum error_level level,
	char *filename, char *message, char *context, size_t context_len,
	struct location location);

#endif
