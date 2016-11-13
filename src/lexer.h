#ifndef LEXER_H
#define LEXER_H

#include "inbuf.h"
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
	struct inbuf *inbuf;
	struct symtab *symtab;
	char *line;
	size_t line_len;
	size_t line_size;
	enum lexer_eol_style eol;
	char *c;
};

mcc_error_t lexer_init(struct lexer *lexer);
void lexer_set_inbuf(struct lexer *lexer, struct inbuf *buf);
void lexer_set_symtab(struct lexer *lexer, struct symtab *symtab);
mcc_error_t lexer_next(struct lexer *lexer, struct tokinfo *tokinfo);
void lexer_free(struct lexer *lexer);
void lexer_dump_token(struct tokinfo *token);

#endif
