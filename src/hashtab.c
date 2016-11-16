#include "hashtab.h"
#include <assert.h>
#include <string.h>


/*
 * TODO Use of this hashing function is not backed up by any analysis of its
 *      performance. Measure and, if necessary, improve the function.
 *
 * Taken from http://www.cse.yorku.ca/~oz/hash.html#sdbm.
 */
static inline uint64_t hashtab_hash(const char *key)
{
	uint64_t hash = 0;
	int c;

	while ((c = *key++))
		hash = c + (hash << 6) + (hash << 16) - hash;

	return hash;
}


static void hashtab_insert_node(struct hashtab *hashtab, struct hashnode *new_node)
{
	uint64_t hash = hashtab_hash(new_node->key) % hashtab->size;
	struct hashnode *node;
	
	node = &hashtab->table[hash];
	new_node->next = node->next;
	node->next = new_node;
}


static bool hashtab_resize(struct hashtab *hashtab, size_t new_size)
{
	struct hashnode *old_table;
	size_t old_size;
	struct hashnode *node;
	size_t i;

	old_table = hashtab->table;
	old_size = hashtab->size;

	hashtab->table = calloc(new_size, sizeof(*hashtab->table));
	if (!hashtab->table) {
		hashtab->table = old_table;
		return false;
	}
	hashtab->size = new_size;

	for (i = 0; i < old_size; i++) {
		node = old_table[i].next;

		while (node) {
			hashtab_insert_node(hashtab, node);
			node = node->next;
		}
	}

	free(old_table);
	return true;
}


bool hashtab_init(struct hashtab *hashtab, struct objpool *pool, size_t init_size)
{
	hashtab->table = NULL;
	hashtab->pool = pool;
	hashtab->size = 0;
	hashtab->count = 0;

	return hashtab_resize(hashtab, init_size);
}


size_t hashtab_count(struct hashtab *hashtab)
{
	return hashtab->count;
}


bool hashtab_contains(struct hashtab *hashtab, const char *key)
{
	return hashtab_search(hashtab, key) != NULL;
}


void *hashtab_search(struct hashtab *hashtab, const char *key)
{
	uint64_t hash;
	struct hashnode *node;

	hash = hashtab_hash(key) % hashtab->size;
	node = &hashtab->table[hash];
	node->key = key; /* used by hashtab_next */

	return hashtab_next(hashtab, node);
}


void *hashtab_next(struct hashtab *hashtab, struct hashnode *node)
{
	struct hashnode *cur = node->next;

	while (cur != NULL) {
		if (strcmp(cur->key, node->key) == 0)
			return cur;

		cur = cur->next;
	}

	return NULL;
}


void *hashtab_insert(struct hashtab *hashtab, const char *key)
{
	struct hashnode *new_node;
	float load;

	load = hashtab->count / hashtab->size;
	if (load > 0.5)
		if (!hashtab_resize(hashtab, 2 * hashtab->size))
			return false;

	new_node = objpool_alloc(hashtab->pool);
	if (!new_node)
		return NULL;

	new_node->key = key;

	hashtab_insert_node(hashtab, new_node);
	hashtab->count++;

	return new_node;
}


bool hashtab_remove(struct hashtab *hashtab, struct hashnode *node)
{
	uint64_t hash;
	struct hashnode *cur;

	hash = hashtab_hash(node->key);

	cur = &hashtab->table[hash];
	while (cur && cur->next != node)
		cur = cur->next;

	if (cur->next != node)
		return false;

	cur->next = node->next;
	objpool_dealloc(hashtab->pool, node);

	hashtab->count--;
	assert(hashtab->count >= 0);

	return true;
}


void hashtab_free(struct hashtab *hashtab)
{
	free(hashtab->table);
}
