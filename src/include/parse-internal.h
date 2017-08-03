#ifndef PARSE_INTERNAL_H
#define PARSE_INTERNAL_H

#include "parse.h"

void parser_next(struct parser *parser);
void parser_skip(struct parser *parser);
void parser_next_push(struct parser *parser, struct toklist *toklist);
bool parser_is_eof(struct parser *parser);
void parser_skip_rest(struct parser *parser);

struct ast_node *parser_parse_decl(struct parser *parser);
void dump_decln(struct ast_node *decln);

#endif
