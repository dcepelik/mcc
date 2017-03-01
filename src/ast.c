#include "ast.h"


struct ast_node *ast_node_new(struct context *ctx)
{
	struct ast_node *new = objpool_alloc(&ctx->ast_node_pool);
	ast_node_init(new);

	return new;
}


void ast_node_init(struct ast_node *node)
{
	node->flags = 0;
}
