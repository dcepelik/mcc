#ifndef CONTEXT_H
#define CONTEXT_H

#include "symbol.h"
#include "errlist.h"
#include "mempool.h"
#include "objpool.h"

/*
 * Translation unit context. Owns objects and memory needed by multiple
 * components involved in the translation process.
 */
struct context
{
	struct symtab symtab;		/* symbol table */
	struct errlist errlist;		/* error list */

	struct objpool token_pool;	/* objpool for struct token */
	struct mempool token_data;	/* mempool for misc token data */
	struct objpool ast_node_pool;	/* mempool for AST nodes */
	struct objpool ast_node_2_pool;	/* mempool for AST nodes (new) */
	struct mempool ast_data;	/* mempool for mist AST data */
};

void context_init(struct context *ctx);
void context_free(struct context *ctx);

#endif
