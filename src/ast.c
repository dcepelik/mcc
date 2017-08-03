#include "ast.h"


struct ast_node *ast_node_new(struct context *ctx, enum ast_node_type type)
{
	struct ast_node *new = objpool_alloc(&ctx->ast_node_pool);
	new->type = type;
	return new;
}
