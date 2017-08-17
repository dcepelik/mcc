#include "ast.h"


struct ast_node *ast_node_new(struct context *ctx, enum ast_node_type type)
{
	struct ast_node *new = objpool_alloc(&ctx->ast_node_pool);
	new->type = type;
	return new;
}


struct ast_expr *ast_expr_new(struct context *ctx, enum expr_type type)
{
	struct ast_expr *expr = objpool_alloc(&ctx->exprs);
	expr->type = type;
	return expr;
}
