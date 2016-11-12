#ifndef MEMPOOL_H
#define MEMPOOL_H

#include "error.h"
#include <assert.h>
#include <stdlib.h>

struct mempool
{
	void *mry;
	size_t item_size;
	size_t block_size;
	size_t items_per_block;
	size_t num_allocated;
	size_t num_blocks;
	struct mempool_block *first_block;
	struct mempool_unused *first_unused;
};

struct mempool_block
{
	struct mempool_block *next;
};

struct mempool_unused
{
	struct mempool_unused *next;
};

void mempool_init(struct mempool *pool, size_t item_size, size_t items_per_block);
void *mempool_alloc(struct mempool *pool);
void mempool_dealloc(struct mempool *pool, void *mem);
void mempool_free(struct mempool *pool);

#endif
