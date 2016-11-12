#include "debug.h"
#include "mempool.h"
#include "symtab.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

/*
 * TODO Use of this hashing function is not backed up by ani analysis of its
 *      performance. Measure and, if necessary, improve the function.
 *
 * Taken from http://www.cse.yorku.ca/~oz/hash.html#sdbm.
 */
static inline uint64_t hash(const char *name)
{
	uint64_t hash = 0;
	int c;

	while ((c = *name++))
		hash = c + (hash << 6) + (hash << 16) - hash;

	return hash;
}


struct symbol *symtab_search(struct symtab *symtab, const char *name)
{
	size_t index;
	struct symbol *sym;

	index = hash(name) % symtab->size;
	sym = symtab->table[index];

	while (sym != NULL) {
		if (strcmp(name, sym->name) == 0)
			return sym;

		sym = sym->next;
	}

	return NULL;
}


bool symtab_contains(struct symtab *symtab, const char *name)
{
	return symtab_search(symtab, name) != NULL;
}


mcc_error_t symtab_resize(struct symtab *symtab, size_t new_size)
{
	size_t i;

	symtab->table = realloc(symtab->table, new_size * sizeof(struct symbol));
	if (symtab->table == NULL)
		return MCC_ERROR_NOMEM;

	for (i = symtab->size; i < new_size; i++)
		symtab->table[i] = NULL;

	symtab->size = new_size;

	return MCC_ERROR_OK;
}


struct symbol *symtab_insert(struct symtab *symtab, const char *name)
{
	float load;
	size_t index;
	struct symbol *prev;
	struct symbol *new;

	new = mempool_alloc(&symtab->symbols);

	new->name = strdup(name);
	if (new->name == NULL) {
		mempool_dealloc(&symtab->symbols, new);
		return NULL;
	}
	new->next = NULL;

	load = (symtab->count + 1) / symtab->size;
	if (load > 0.5) {
		if (symtab_resize(symtab, 2 * symtab->size) != MCC_ERROR_OK)
			return NULL;
	}

	assert(symtab->table != NULL);

	index = hash(name) % symtab->size;
	
	if (symtab->table[index] == NULL) {
		symtab->table[index] = new;
	}
	else {
		prev = symtab->table[index];
		while (prev->next != NULL)
			prev = prev->next;

		prev->next = new;
	}

	symtab->count++;

	return new;
}


mcc_error_t symtab_init(struct symtab *symtab)
{
	mempool_init(&symtab->symbols, sizeof(struct symbol), 64);

	symtab->size = 0;
	symtab->count = 0;
	symtab->table = NULL;

	return symtab_resize(symtab, 128);
}


void symtab_free(struct symtab *symtab)
{
	mempool_free(&symtab->symbols);
	free(symtab->table);
}
