/*
 * hashtab:
 * Simple hashtable.
 */

#ifndef HASHTAB_H
#define HASHTAB_H

#include "objpool.h"
#include "mempool.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

struct hashtab
{
	struct hashnode *table;
	struct objpool *pool;
	struct mempool keys;
	size_t count;
	size_t size;
};

struct hashnode
{
	char *key;
	struct hashnode *next;
};

bool hashtab_init(struct hashtab *table, struct objpool *pool, size_t init_size);
size_t hashtab_count(struct hashtab *table);
bool hashtab_contains(struct hashtab *table, char *key);
void *hashtab_search(struct hashtab *table, char *key);
void *hashtab_next(struct hashtab *table, struct hashnode *node);
void *hashtab_insert(struct hashtab *table, char *key);
bool hashtab_remove(struct hashtab *table, struct hashnode *node);
void hashtab_free(struct hashtab *table);

#endif
