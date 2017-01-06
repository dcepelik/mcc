#ifndef AST_H
#define AST_H

#include "context.h"

enum ast_node_type
{
	AST_NODE_TRANSLATION_UNIT,
	AST_NODE_FUNC_DEF,
	AST_NODE_IDENTIFIER,
	AST_NODE_DECLSPEC,
	AST_NODE_PTR_TO,
	AST_NODE_INT,
};

struct ast_node
{
	enum ast_node_type type;
	struct ast_node *children;
	struct ast_node *target;
};

void ast_node_init(struct ast_node *node);
struct ast_node *ast_node_new(struct context *ctx);

struct ast
{
	struct ast_node root;
};

#endif
