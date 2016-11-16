#include "debug.h"
#include "mempool.h"
#include "symtab.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>


bool symtab_init(struct symtab *symtab)
{
	objpool_init(&symtab->symbol_pool, sizeof(struct symbol), 16);
	mempool_init(&symtab->symdata_pool, 1024);
	return hashtab_init(&symtab->table, &symtab->symbol_pool, 64);
}


struct symbol *symtab_search(struct symtab *symtab, const char *name)
{
	return hashtab_search(&symtab->table, name);
}


bool symtab_contains(struct symtab *symtab, const char *name)
{
	return hashtab_contains(&symtab->table, name);
}


struct symbol *symtab_insert(struct symtab *symtab, const char *name)
{
	return hashtab_insert(&symtab->table, name);
}


void *symtab_alloc(struct symtab *symtab, size_t size)
{
	return mempool_alloc(&symtab->symdata_pool, size);
}


void symtab_free(struct symtab *symtab)
{
	hashtab_free(&symtab->table);
	objpool_free(&symtab->symbol_pool);
	mempool_free(&symtab->symdata_pool);
}
