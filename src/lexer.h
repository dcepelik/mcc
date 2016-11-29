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
	struct mempool token_data;	/* mempool for token data (strings mostly) */
	struct inbuf *inbuf;		/* input buffer */
	struct strbuf strbuf;		/* buffer for string accumulation */
	char *c;			/* current character pointer (within line) */
	struct strbuf linebuf;		/* buffer for current logical line */
	enum lexer_eol_style eol_style;	/* end-of-line style used in inbuf */
	bool inside_include;		/* are we lexing in an #include? */
	bool next_at_bol;		/* is next token at BOL? */
	bool first_token;		/* have we produced the first token already? */

	struct symtab *symtab;		/* symbol table */

	/* global data */
	struct tokinfo eol;		/* tokinfo of an EOL token */
	struct tokinfo eof;		/* tokinfo of an EOF token */
};

mcc_error_t lexer_init(struct lexer *lexer);
void lexer_set_inbuf(struct lexer *lexer, struct inbuf *buf);
struct tokinfo *lexer_next(struct lexer *lexer);
void lexer_set_symtab(struct lexer *lexer, struct symtab *symtab);
void lexer_free(struct lexer *lexer);
void lexer_dump_token(struct tokinfo *token);

#endif
