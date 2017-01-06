#include "ast.h"


struct ast_node *ast_node_new(struct context *ctx)
{
	return objpool_alloc(&ctx->ast_node_pool);
}


void ast_node_init(struct ast_node *node)
{
	node->target = NULL;
}
