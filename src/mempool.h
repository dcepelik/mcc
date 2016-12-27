/*
 * mempool:
 * Variable-size memory allocator without support for individual deallocation.
 */

#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <stdlib.h>

struct mempool_block
{
	struct mempool_block *prev;	/* previous block in the chain */
	size_t alloc_size;		/* allocated size */
	size_t size;			/* usable size */
};

struct mempool_chain
{
	struct mempool_block *last;	/* last block */
	size_t last_free;		/* free memory in last block */
	size_t total_size;		/* total size of the chain */
	size_t num_blocks;		/* number of blocks in the chain */
};

struct mempool
{
	struct mempool_chain small;	/* chain of blocks for small objects */
	struct mempool_chain big;	/* chain of blocks for big objects */
	struct mempool_chain unused;	/* chain of unused blocks */
	size_t block_size;		/* size of small objects block */
	size_t small_treshold;		/* maximum size of a small object */
};

void mempool_init(struct mempool *pool, size_t block_size);
void mempool_free(struct mempool *pool);

void *mempool_alloc(struct mempool *pool, size_t size);
char *mempool_memcpy(struct mempool *pool, char *src, size_t len);
char *mempool_strdup(struct mempool *pool, char *str);

void mempool_print_stats(struct mempool *pool);

#endif
