/*
 * hashtab:
 * Simple hashtable.
 */

#ifndef HASHTAB_H
#define HASHTAB_H

#include "objpool.h"
#include "mempool.h"
#include <stdbool.h>
#include <stdlib.h>

struct hashtab
{
	struct hashnode *table;
	struct mempool keys;
	size_t count;
	size_t size;
};

void hashtab_init(struct hashtab *table, struct objpool *pool, size_t init_size);
void hashtab_free(struct hashtab *table);

size_t hashtab_count(struct hashtab *table);

void *hashtab_insert(struct hashtab *table, char *key, struct hashnode *node);
bool hashtab_remove(struct hashtab *table, struct hashnode *node);

bool hashtab_contains(struct hashtab *table, char *key);
void *hashtab_search(struct hashtab *table, char *key);
void *hashtab_next(struct hashtab *table, struct hashnode *node);

struct hashnode
{
	char *key;
	struct hashnode *next;
};

#endif
