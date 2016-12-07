#ifndef CPPFILE_H
#define CPPFILE_H

#include "error.h"
#include "inbuf.h"
#include "list.h"
#include "mempool.h"
#include "objpool.h"
#include "strbuf.h"
#include "symbol.h"
#include "token.h"
#include <stdlib.h>


struct lexer_state {

};


/* TODO Make this internal and hidden from the backend and the parser. */
/* TODO Inits for lexer and cpp may be better off separated. */
struct cppfile
{
	struct symtab *symtab;		/* symbol table */
	struct objpool token_pool;	/* objpool for struct token */
	struct mempool token_data;	/* mempool for token data (strings mostly) */
	struct objpool macro_pool;	/* objpool for struct macro */

	/* lexer state */
	struct inbuf inbuf;		/* input buffer */
	struct strbuf linebuf;		/* buffer for current logical line */
	char *c;			/* current character within linebuf */
	struct strbuf strbuf;		/* buffer for string accumulation */
	struct location location;	/* location of c within inbuf */

	/* additional lexer flags */
	bool inside_include;		/* are we lexing in an #include? */
	bool next_at_bol;		/* is next token at BOL? */
	bool first_token;		/* have we produced the first token already? */
	bool had_whitespace;		/* have we read any whitespace just before lexing current token? */

	/* preprocessor state */
	struct list tokens;		/* token queue */
	struct token *token;		/* last popped token */
	struct list ifs;		/* if directive control stack */
	struct cppfile *included_file;	/* currently included file */
};

struct cppfile *cppfile_new();
void cppfile_delete(struct cppfile *file);

mcc_error_t cppfile_open(struct cppfile *file, char *filename);
void cppfile_close(struct cppfile *file);

void cppfile_set_symtab(struct cppfile *file, struct symtab *table);

void cppfile_error(struct cppfile *file, char *fmt, ...);
void cppfile_warning(struct cppfile *file, char *fmt, ...);

#endif
