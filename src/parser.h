#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "context.h"
#include "cpp.h"

struct parser
{
	struct context ctx;
	struct cpp *cpp;
	struct token *token;
};

void parser_init(struct parser *parser);
void parser_free(struct parser *parser);

void parser_build_ast(struct parser *parser, struct ast *tree, char *cfile);

#endif
