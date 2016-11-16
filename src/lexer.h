#ifndef LEXER_H
#define LEXER_H

/*
 * Lexer for translation phases 1-3.
 *
 * TODO Process universal character names
 * TODO Handle various encoding-related issues
 */

#include "inbuf.h"
#include "mempool.h"
#include "objpool.h"
#include "strbuf.h"
#include "tokinfo.h"
#include <stdbool.h>

enum lexer_eol_style
{
	LEXER_EOL_MAC,
	LEXER_EOL_MSDOS,
	LEXER_EOL_UNIX,
	LEXER_EOL_UNKNOWN,
};

struct lexer
{
	struct objpool tokinfo_pool;	/* objpool for struct tokinfo */
	struct mempool token_data;	/* mempool for token data */
	struct strbuf linebuf;		/* current logical line buffer */
	struct strbuf strbuf;		/* buffer to accumulate various strings */
	struct inbuf *inbuf;		/* input buffer being tokenized */
	char *c;			/* pointer to current character */
	enum lexer_eol_style eol;	/* end-of-line style in use */
};

mcc_error_t lexer_init(struct lexer *lexer);
void lexer_set_inbuf(struct lexer *lexer, struct inbuf *buf);
mcc_error_t lexer_next(struct lexer *lexer, struct tokinfo *tokinfo);
void lexer_free(struct lexer *lexer);
void lexer_dump_token(struct tokinfo *token);

#endif
