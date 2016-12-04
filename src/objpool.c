#include "debug.h"
#include "error.h"
#include "objpool.h"
#include <assert.h>


void objpool_init(struct objpool *objpool, size_t obj_size, size_t objs_per_block)
{
	size_t min_size;

	assert(obj_size >= sizeof(struct objpool_unused));
	assert(objs_per_block > 1);

	objpool->obj_size = obj_size;
	objpool->objs_per_block = objs_per_block;
	objpool->first_block = NULL;
	objpool->first_unused = NULL;
	objpool->num_objs = 0;
	objpool->num_blocks = 0;

	min_size = obj_size * objs_per_block + sizeof(struct objpool_block);
	objpool->block_size = min_size;

	while (min_size > objpool->block_size)
		objpool->block_size *= 2;
}


static mcc_error_t alloc_new_block(struct objpool *pool)
{
	struct objpool_block *new_block;
	struct objpool_unused *new_unused;
	void *mem;
	size_t i;

	new_block = malloc(pool->block_size);
	if (new_block == NULL)
		return MCC_ERROR_NOMEM;

	DEBUG_PRINTF("Allocated new block, alloc_size = %zu B", pool->block_size);

	new_block->next = pool->first_block;
	pool->first_block = new_block;

	mem = new_block + 1; /* mem points right after struct objpool_block */

	for (i = 0; i < pool->objs_per_block; i++) {
		new_unused = mem;
		new_unused->next = pool->first_unused;
		pool->first_unused = new_unused;

		mem = (unsigned char *)mem + pool->obj_size;
	}

	pool->num_blocks++;

	return MCC_ERROR_OK;
}


void *objpool_alloc(struct objpool *pool)
{
	DEBUG_MSG("Allocating new object from pool");

	void *mem;

	if (pool->first_unused == NULL)
		if (alloc_new_block(pool) != MCC_ERROR_OK)
			return NULL;

	assert(pool->first_unused != NULL);

	mem = pool->first_unused;
	pool->first_unused = pool->first_unused->next;

	pool->num_objs++;

	return mem;
}


void objpool_dealloc(struct objpool *pool, void *mem)
{
	struct objpool_unused *unused = mem;

	unused->next = pool->first_unused;
	pool->first_unused = unused;

	pool->num_objs--;

	assert(pool->num_objs >= 0);
}


void objpool_free(struct objpool *pool)
{
	struct objpool_block *block = pool->first_block;

	while (pool->first_block) {
		block = pool->first_block;
		pool->first_block = pool->first_block->next;

		free(block);
	}
}


void objpool_print_stats(struct objpool *pool)
{
	printf("objpool stats: %lu objs, %lu blocks\n", pool->num_objs, pool->num_blocks);
}
