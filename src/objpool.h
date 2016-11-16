/*
 * objpool:
 * Simple fixed-size allocator.
 *
 * TODO objpool_alloc should return aligned memory
 */

#ifndef OBJPOOL_H
#define OBJPOOL_H

#include <stdlib.h>

struct objpool
{
	void *mry;
	struct objpool_block *first_block;
	struct objpool_unused *first_unused;
	size_t obj_size;
	size_t block_size;
	size_t objs_per_block;
	size_t num_objs;
	size_t num_blocks;
};

struct objpool_block
{
	struct objpool_block *next;
};

struct objpool_unused
{
	struct objpool_unused *next;
};

void objpool_init(struct objpool *pool, size_t obj_size, size_t objs_per_block);
void *objpool_alloc(struct objpool *pool);
void objpool_dealloc(struct objpool *pool, void *mem);
void objpool_free(struct objpool *pool);

#endif
