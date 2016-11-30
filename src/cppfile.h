#ifndef CPPFILE_H
#define CPPFILE_H

#include "error.h"
#include "inbuf.h"
#include "list.h"
#include "mempool.h"
#include "objpool.h"
#include "strbuf.h"
#include "symtab.h"
#include "tokinfo.h"
#include <stdlib.h>


/* TODO Make this internal and hidden from the backend and the parser. */
/* TODO Inits for lexer and cpp may be better off separated. */
struct cppfile
{
	struct objpool tokinfo_pool;	/* objpool for struct tokinfo */
	struct mempool token_data;	/* mempool for token data (strings mostly) */
	struct strbuf linebuf;		/* buffer for current logical line */
	struct strbuf strbuf;		/* buffer for string accumulation */
	struct inbuf inbuf;		/* input buffer */
	char *c;			/* current character pointer (within line) */
	struct symtab *symtab;		/* symbol table */
	bool inside_include;		/* are we lexing in an #include? */
	bool next_at_bol;		/* is next token at BOL? */
	bool first_token;		/* have we produced the first token already? */

	struct list tokens;		/* token list */
	struct tokinfo *cur;		/* last popped tokinfo */

	struct location location;	/* current location */

	struct list ifs;		/* if directive stack */
	bool skip;
};

struct cppfile *cppfile_new();
void cppfile_delete(struct cppfile *file);

mcc_error_t cppfile_open(struct cppfile *file, char *filename);
void cppfile_close(struct cppfile *file);

void cppfile_set_symtab(struct cppfile *file, struct symtab *table);

void cppfile_error(struct cppfile *file, char *fmt, ...);
void cppfile_warning(struct cppfile *file, char *fmt, ...);

#endif
