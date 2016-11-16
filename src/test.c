#include "hashtab.h"
#include "objpool.h"
#include <assert.h>
#include <stdio.h>

struct my_item
{
	struct hashnode node;
	int i;
};


int main(void)
{
	struct hashtab hashtab;
	struct objpool pool;
	struct my_item *item;
	size_t i;

	char *a;

	objpool_init(&pool, sizeof(struct my_item), 10);
	hashtab_init(&hashtab, &pool, 2500);

	for (i = 0; i < 1024; i++) {
		fprintf(stderr, "%lu\n", i);
		item = hashtab_insert(&hashtab, "a");
		item->i = i;
	}

	i = 1023;
	item = hashtab_search(&hashtab, "a");
	while (item) {
		printf("item ID: %i\n", item->i);
		assert(item);
		assert(item->i == i);
		i--;
		item = hashtab_next(&hashtab, item);
	}

	printf("%lu objects in %lu blocks, total %zu bytes of memory\n", pool.num_objs, pool.num_blocks, pool.num_blocks * pool.block_size);

	while (hashtab_count(&hashtab) > 0) {
		item = hashtab_search(&hashtab, "a");
		hashtab_remove(&hashtab, item);
		printf("# of items: %lu\n", hashtab_count(&hashtab));
	}

	printf("%lu objects in %lu blocks, total %zu bytes of memory\n", pool.num_objs, pool.num_blocks, pool.num_blocks * pool.block_size);

	hashtab_free(&hashtab);
	objpool_free(&pool);

	return 0;
}
