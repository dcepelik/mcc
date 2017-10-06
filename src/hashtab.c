#include "debug.h"
#include "hashtab.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

/*
 * TODO Use of this hashing function is not backed up by any analysis of its
 *      performance. Measure and, if necessary, improve the function.
 *
 * Taken from http://www.cse.yorku.ca/~oz/hash.html#sdbm.
 */
static inline uint64_t hashtab_hash(char *key)
{
	uint64_t hash = 0;
	int c;

	while ((c = *key++))
		hash = c + (hash << 6) + (hash << 16) - hash;

	return hash;
}

static void hashtab_insert_internal(struct hashtab *hashtab, struct hashnode *new_node)
{
	uint64_t hash = hashtab_hash(new_node->key) % hashtab->size;
	struct hashnode *node;
	
	node = &hashtab->table[hash];
	new_node->next = node->next;
	node->next = new_node;
}

static void hashtab_resize(struct hashtab *hashtab, size_t new_size)
{
	struct hashnode *old_table;
	size_t old_size;
	struct hashnode *node, *tmp_node;
	size_t i;

	old_table = hashtab->table;
	old_size = hashtab->size;

	hashtab->table = calloc(new_size, sizeof(*hashtab->table));
	hashtab->size = new_size;

	for (i = 0; i < old_size; i++) {
		node = old_table[i].next;

		while (node) {
			tmp_node = node->next;
			hashtab_insert_internal(hashtab, node);
			node = tmp_node;
		}
	}

	free(old_table);
}

void hashtab_init(struct hashtab *hashtab, struct objpool *pool, size_t init_size)
{
	(void) pool; /* TODO why unused? */

	hashtab->table = NULL;
	hashtab->size = 0;
	hashtab->count = 0;
	mempool_init(&hashtab->keys, 1024);
	hashtab_resize(hashtab, init_size);
}

void hashtab_free(struct hashtab *hashtab)
{
	mempool_free(&hashtab->keys);
	free(hashtab->table);
}

void *hashtab_insert(struct hashtab *hashtab, char *key, struct hashnode *node)
{
	float load;
	char *key_copy;
	size_t key_len;

	load = hashtab->count / hashtab->size;
	if (load > 0.5)
		hashtab_resize(hashtab, 2 * hashtab->size);

	key_len = strlen(key);
	key_copy = mempool_alloc(&hashtab->keys, key_len + 1);
	strncpy(key_copy, key, key_len + 1);

	node->key = key_copy;

	hashtab_insert_internal(hashtab, node);
	hashtab->count++;

	return node;
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

	assert(hashtab->count > 0);
	hashtab->count--;

	return true;
}

size_t hashtab_count(struct hashtab *hashtab)
{
	return hashtab->count;
}

bool hashtab_contains(struct hashtab *hashtab, char *key)
{
	return hashtab_search(hashtab, key) != NULL;
}

void *hashtab_search(struct hashtab *hashtab, char *key)
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
	(void) hashtab;

	struct hashnode *cur = node->next;

	while (cur != NULL) {
		if (strcmp(cur->key, node->key) == 0)
			return cur;

		cur = cur->next;
	}

	return NULL;
}
