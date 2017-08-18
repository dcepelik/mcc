#include "context.h"
#include "ast.h"

#define TOKEN_POOL_BLOCK_SIZE	256
#define TOKEN_DATA_BLOCK_SIZE	1024


void context_init(struct context *ctx)
{
	mempool_init(&ctx->token_data, TOKEN_DATA_BLOCK_SIZE);
	objpool_init(&ctx->token_pool, sizeof(struct token), TOKEN_POOL_BLOCK_SIZE);
	symtab_init(&ctx->symtab);
	errlist_init(&ctx->errlist);
	objpool_init(&ctx->exprs, sizeof(struct ast_expr), 16);
}


void context_free(struct context *ctx)
{
	mempool_free(&ctx->token_data);
	objpool_free(&ctx->token_pool);
	errlist_free(&ctx->errlist);
	symtab_free(&ctx->symtab);
	objpool_free(&ctx->exprs);
}
