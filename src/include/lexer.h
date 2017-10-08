/*
 * Lexer for translation phases 1-3.
 *
 * TODO Process universal character names
 * TODO Handle various encoding-related issues
 *
 */

#ifndef LEXER_H
#define LEXER_H

#include "error.h"
#include "inbuf.h"
#include "strbuf.h"
#include "token.h"

/*
 * Lexer state encapsulation.
 */
struct lexer
{
	struct context *ctx;
	char *filename;			/* TODO */

	struct inbuf *inbuf;		/* input buffer */
	struct strbuf linebuf;		/* buffer for current logical line */
	char *c;			/* current character within linebuf */
	struct location location;	/* location of c within inbuf */
	struct strbuf strbuf;		/* buffer for various string accumulation */
	struct strbuf spelling;		/* buffer for token spelling */
	char *spelling_start;		/* start of current token's spelling */

	/* flags */
	bool inside_include;		/* are we various lexing in an #include? */
	bool next_at_bol;		/* is next token at BOL? */
	bool first_token;		/* first token of the line produced? */
	bool had_whitespace;		/* whitespace before current token? */
};

void lexer_init(struct lexer *lexer, struct context *ctx, struct inbuf *inbuf);
void lexer_free(struct lexer *lexer);
void lexer_next(struct lexer *lexer, struct token *token);

#endif
