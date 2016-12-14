/*
 * Lexer for translation phases 1-3.
 *
 * TODO Process universal character names
 * TODO Handle various encoding-related issues
 *
 */

#ifndef LEXER_H
#define LEXER_H

#include "strbuf.h"
#include "inbuf.h"
#include "token.h"

/*
 * Lexer state encapsulation.
 */
struct lexer
{
	struct cpp *cpp;		/* CPP object owning this lexer */
	char *filename;			/* TODO */

	struct inbuf inbuf;		/* input buffer */
	struct strbuf linebuf;		/* buffer for current logical line */
	char *c;			/* current character within linebuf */
	struct location location;	/* location of c within inbuf */
	struct strbuf strbuf;		/* buffer for string accumulation */
	struct strbuf spelling;		/* buffer for token spelling */

	/* flags */
	bool inside_include;		/* are we lexing in an #include? */
	bool emit_eols;
	bool next_at_bol;		/* is next token at BOL? */
	bool first_token;		/* first token of the line produced? */
	bool had_whitespace;		/* whitespace before current token? */
};

struct cpp *cpp;

mcc_error_t lexer_init(struct lexer *lexer, struct cpp *cpp, char *filename);
void lexer_free(struct lexer *lexer);
struct token *lexer_next(struct lexer *lexer);

#endif
