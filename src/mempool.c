#include "mempool.h"


void mempool_init(struct mempool *mempool, size_t item_size, size_t items_per_block)
{
	size_t min_size;

	assert(item_size >= sizeof(struct mempool_unused));
	assert(items_per_block > 1);

	mempool->item_size = item_size;
	mempool->items_per_block = items_per_block;
	mempool->first_block = NULL;
	mempool->first_unused = NULL;
	mempool->num_allocated = 0;

	min_size = item_size * items_per_block + sizeof(struct mempool_block);
	mempool->block_size = 8; //512 * 1024;

	while (min_size > mempool->block_size)
		mempool->block_size *= 2;
}


static mcc_error_t alloc_new_block(struct mempool *pool)
{
	struct mempool_block *new_block;
	struct mempool_unused *new_unused;
	void *mem;
	size_t i;

	new_block = malloc(pool->block_size);
	if (new_block == NULL)
		return MCC_ERROR_NOMEM;

	new_block->next = pool->first_block;
	pool->first_block = new_block;

	mem = new_block + 1; /* mem points right after struct mempool_block */

	for (i = 0; i < pool->items_per_block; i++) {
		new_unused = mem;
		new_unused->next = pool->first_unused;
		pool->first_unused = new_unused;

		mem += pool->item_size;
	}

	pool->num_blocks++;

	return MCC_ERROR_OK;
}


void *mempool_alloc(struct mempool *pool)
{
	void *mem;

	if (pool->first_unused == NULL)
		if (alloc_new_block(pool) != MCC_ERROR_OK)
			return NULL;

	assert(pool->first_unused != NULL);

	mem = pool->first_unused;
	pool->first_unused = pool->first_unused->next;

	pool->num_allocated++;

	return mem;
}


void mempool_dealloc(struct mempool *pool, void *mem)
{
	struct mempool_unused *unused = mem;

	unused->next = pool->first_unused;
	pool->first_unused = unused;

	pool->num_allocated--;

	assert(pool->num_allocated >= 0);
}


void mempool_free(struct mempool *pool)
{
	struct mempool_block *block = pool->first_block;

	while (pool->first_block) {
		block = pool->first_block;
		pool->first_block = pool->first_block->next;

		free(block);
	}
}
