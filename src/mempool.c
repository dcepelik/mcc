#include "mempool.h"
#include <assert.h>
#include "debug.h"


static void mempool_init_chain(struct mempool_chain *chain)
{
	chain->last = NULL;
	chain->last_free = 0;
	chain->num_blocks = 0;
	chain->total_size = 0;
}


static void mempool_free_chain(struct mempool_chain *chain)
{
	struct mempool_block *block;
	void *mem;

	while (chain->last) {
		block = chain->last;
		chain->last = chain->last->prev;
		
		mem = (unsigned char *)block - block->size;
		DEBUG_PRINTF("About to free raw block at %p", mem);

		chain->num_blocks--;
		chain->total_size -= block->alloc_size;
		free(mem);
	}
}


struct mempool_block *mempool_new_block(struct mempool_chain *chain, size_t size)
{
	// TODO Alignment

	struct mempool_block *new_block;
	size_t alloc_size;
	void *mem;

	alloc_size = size + sizeof(*new_block);
	
	mem = malloc(alloc_size);
	if (!mem)
		return NULL;

	DEBUG_PRINTF("Alocated new block, alloc_size = %zu B and size = %zu B",
		alloc_size, size);
	DEBUG_PRINTF("Raw block starts at %p", mem);

	new_block = (struct mempool_block *)((unsigned char *)mem + size);

	DEBUG_PRINTF("Block trailer resides at %p", (void *)new_block);

	new_block->alloc_size = alloc_size;
	new_block->size = size;

	new_block->prev = chain->last;
	chain->last = new_block;

	chain->total_size += alloc_size;
	chain->num_blocks++;
	chain->last_free = size;

	return new_block;
}


static inline void *mempool_alloc_chain(struct mempool_chain *chain, size_t size)
{
	size_t free_size;

	free_size = chain->last_free;
	assert(free_size >= size);

	chain->last_free -= size;
	assert(chain->last_free >= 0);

	return (unsigned char *)chain->last - free_size;
}


void mempool_init(struct mempool *pool, size_t block_size)
{
	pool->block_size = block_size;
	pool->small_treshold = block_size / 2;

	mempool_init_chain(&pool->small);
	mempool_init_chain(&pool->big);
}


void *mempool_alloc(struct mempool *pool, size_t size)
{
	DEBUG_PRINTF("Alloc request, size = %zu B", size);

	if (size <= pool->small_treshold) {
		if (pool->small.last_free < size) {
			DEBUG_MSG("Allocating block in small chain");
			mempool_new_block(&pool->small, pool->block_size);
		}

		return mempool_alloc_chain(&pool->small, size);
	}
	else {
		DEBUG_MSG("Allocating block in big chain");
		mempool_new_block(&pool->big, size);
		return mempool_alloc_chain(&pool->big, size);
	}
}


void mempool_free(struct mempool *pool)
{
	mempool_free_chain(&pool->small);
	mempool_free_chain(&pool->big);
}


static void mempool_print_chain_stats(struct mempool_chain *chain)
{
	printf("%lu blocks, %lu B total\n", chain->num_blocks, chain->total_size);
}


void mempool_print_stats(struct mempool *pool)
{
	printf("Mempool stats:\n");
	printf("\tBig objects chain: ");
	mempool_print_chain_stats(&pool->big);
	printf("\tSmall objects chain: ");
	mempool_print_chain_stats(&pool->small);
}
