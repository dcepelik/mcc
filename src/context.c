#include "context.h"
#include "ast.h"

#define TOKEN_POOL_BLOCK_SIZE	256
#define TOKEN_DATA_BLOCK_SIZE	1024
#define AST_DATA_BLOCK_SIZE	1024


void context_init(struct context *ctx)
{
	mempool_init(&ctx->token_data, TOKEN_DATA_BLOCK_SIZE);
	objpool_init(&ctx->token_pool, sizeof(struct token), TOKEN_POOL_BLOCK_SIZE);
	symtab_init(&ctx->symtab);
	errlist_init(&ctx->errlist);
	objpool_init(&ctx->ast_node_pool, sizeof(struct ast_node), 16); /* 1024: bugs */
	objpool_init(&ctx->ast_node_2_pool, sizeof(struct ast_node_2), 16); /* 1024: bugs */
	mempool_init(&ctx->ast_data, AST_DATA_BLOCK_SIZE);
}


void context_free(struct context *ctx)
{
	mempool_free(&ctx->token_data);
	objpool_free(&ctx->token_pool);
	errlist_free(&ctx->errlist);
	symtab_free(&ctx->symtab);
	objpool_free(&ctx->ast_node_pool);
	objpool_free(&ctx->ast_node_2_pool);
	mempool_free(&ctx->ast_data);
}
