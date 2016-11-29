#ifndef CPPFILE_H
#define CPPFILE_H

#include "error.h"
#include "inbuf.h"
#include "mempool.h"
#include "objpool.h"
#include "strbuf.h"
#include "tokinfo.h"

/* TODO Make this internal and hidden from the backend and the parser. */
struct cppfile
{
	struct objpool tokinfo_pool;	/* objpool for struct tokinfo */
	struct mempool token_data;	/* mempool for token data (strings mostly) */
	struct strbuf linebuf;		/* buffer for current logical line */
	struct strbuf strbuf;		/* buffer for string accumulation */
	struct inbuf inbuf;		/* input buffer */
	char *c;			/* current character pointer (within line) */
	bool inside_include;		/* are we lexing in an #include? */
	bool next_at_bol;		/* is next token at BOL? */
	bool first_token;		/* have we produced the first token already? */
	struct symtab *symtab;		/* symbol table */
};

struct cppfile *cppfile_new();
void cppfile_delete(struct cppfile *file);

mcc_error_t cppfile_open(struct cppfile *file, char *filename);
void cppfile_close(struct cppfile *file);

#endif
