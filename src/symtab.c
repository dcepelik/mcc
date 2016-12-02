#include "debug.h"
#include "mempool.h"
#include "symtab.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>


bool symtab_init(struct symtab *symtab)
{
	objpool_init(&symtab->symbol_pool, sizeof(struct symbol), 1024);
	return hashtab_init(&symtab->table, &symtab->symbol_pool, 256);
}


struct symbol *symtab_search(struct symtab *symtab, char *name)
{
	return hashtab_search(&symtab->table, name);
}


bool symtab_contains(struct symtab *symtab, char *name)
{
	return hashtab_contains(&symtab->table, name);
}


struct symbol *symtab_insert(struct symtab *symtab, char *name)
{
	return hashtab_insert(&symtab->table, name);
}


void symtab_free(struct symtab *symtab)
{
	hashtab_free(&symtab->table);
	objpool_free(&symtab->symbol_pool);
}


char *symbol_get_name(struct symbol *symbol)
{
	return symbol->hashnode.key;
}


const char *symbol_get_type(struct symbol *symbol)
{
	switch (symbol->type)
	{
	case SYMBOL_TYPE_UNDEF:
		return "unknown";

	case SYMBOL_TYPE_CPP_MACRO:
		return "CPP macro";
	
	case SYMBOL_TYPE_CPP_BUILTIN:
		return "CPP built-in macro";

	case SYMBOL_TYPE_CPP_DIRECTIVE:
		return "CPP directive";

	case SYMBOL_TYPE_CPP_MACRO_ARG:
		return "CPP macro arg";
	}

	return NULL;
}
