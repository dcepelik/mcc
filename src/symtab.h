/*
 * symtab:
 * Symbol table.
 */

#ifndef SYMBOL_H
#define SYMBOL_H

#include "objpool.h"
#include "hashtab.h"
#include <stdbool.h>
#include <stdlib.h>

enum symbol_type
{
	SYMBOL_TYPE_CPP_MACRO,		/* C preprocessor macro */
};

struct symbol
{
	struct hashnode node;
	enum symbol_type type;
};

struct symtab
{
	struct objpool symbol_pool;	/* object pool for the symbols */
	struct mempool symdata_pool;	/* memory pool for symbol data */
	struct hashtab table;		/* a hash table */
};

bool symtab_init(struct symtab *table);
bool symtab_contains(struct symtab *table, const char *name);
struct symbol *symtab_search(struct symtab *table, const char *name);
struct symbol *symtab_insert(struct symtab *table, const char *name);
void *symtab_alloc(struct symtab *table, size_t size);
void symtab_free(struct symtab *table);

#endif
