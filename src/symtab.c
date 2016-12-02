#include "debug.h"
#include "mempool.h"
#include "symtab.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>


/*
 * Artificial symbol definition to be kept at the bottom of the symbol
 * definition stack. It represents a symbol which is not defined in
 * current context.
 */
static struct symdef undef_symbol_definition = {
	.type = SYMBOL_TYPE_UNDEF
};


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
	struct symbol *symbol;
	
	symbol = hashtab_insert(&symtab->table, name);
	list_init(&symbol->defs);
	symbol->def = symbol_push_definition(symbol, &undef_symbol_definition);

	return symbol;
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


struct symdef *symbol_push_definition(struct symbol *symbol, struct symdef *symdef)
{
	return symbol->def = (struct symdef *)
		list_insert_first(&symbol->defs, &symdef->list_node);
}


struct symdef *symbol_pop_definition(struct symbol *symbol)
{
	assert(list_first(&symbol->defs) != &undef_symbol_definition);

	return symbol->def = list_remove_first(&symbol->defs);
}


const char *symdef_get_type(struct symdef *symdef)
{
	switch (symdef->type)
	{
	case SYMBOL_TYPE_UNDEF:
		return "undef";

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
