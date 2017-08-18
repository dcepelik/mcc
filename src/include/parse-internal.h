#ifndef PARSE_INTERNAL_H
#define PARSE_INTERNAL_H

#include "parse.h"

void parser_next(struct parser *parser);
void parser_skip(struct parser *parser);
void parser_next_push(struct parser *parser, struct toklist *toklist);
bool parser_is_eof(struct parser *parser);
bool parser_expect(struct parser *parser, enum token_type type);
void parser_skip_rest(struct parser *parser);

void parse_error(struct parser *parser, char *msg, ...);
bool parser_require(struct parser *parser, enum token_type token);

struct ast_decl *parser_parse_decl(struct parser *parser);
void dump_decl(struct ast_decl *decl);

struct ast_expr *parse_expr(struct parser *parser);
void dump_expr(struct ast_expr *expr, struct strbuf *buf);

#endif
