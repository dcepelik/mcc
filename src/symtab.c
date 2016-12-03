#include "debug.h"
#include "mempool.h"
#include "symtab.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>


/*
 * Default symbol definition to be kept at the bottom of the symbol
 * definition stack. It represents a symbol which is not defined in
 * current scope (ie. not in any of the parent scopes either).
 */
static struct symdef undef_symbol_definition = {
	.type = SYMBOL_TYPE_UNDEF
};


struct symtab_scope
{
	struct list_node list_node;	/* for the scope stack */
	struct list defined_symbols;	/* symbols defined within this scope */
};


/*
 * Default scope to be kept at the bottom of the scope stack. It's
 * the root of the scope tree.
 */
static struct symtab_scope global_scope;


static void scope_init(struct symtab_scope *scope)
{
	list_init(&scope->defined_symbols);
}


static void scope_free(struct symtab_scope *scope)
{
	list_free(&scope->defined_symbols);
}


bool symtab_init(struct symtab *symtab)
{
	objpool_init(&symtab->symbol_pool, sizeof(struct symbol), 1024);
	objpool_init(&symtab->scope_pool, sizeof(struct symtab_scope), 64);
	list_init(&symtab->scopes);

	scope_init(&global_scope);
	list_insert_first(&symtab->scopes, &global_scope.list_node);

	return hashtab_init(&symtab->table, &symtab->symbol_pool, 256);
}


void symtab_free(struct symtab *symtab)
{
	hashtab_free(&symtab->table);
	objpool_free(&symtab->symbol_pool);
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
	symbol->def = symbol_push_definition(symtab, symbol, &undef_symbol_definition);

	return symbol;
}


char *symbol_get_name(struct symbol *symbol)
{
	return symbol->hashnode.key;
}


void symbol_scope_begin(struct symtab *table)
{
	struct symtab_scope *scope = objpool_alloc(&table->scope_pool);
	scope_init(scope);
	list_insert_last(&table->scopes, &scope->list_node);
}


void symbol_scope_end(struct symtab *table)
{
	assert(list_first(&table->scopes) != &global_scope);

	struct symtab_scope *scope;
	struct symbol *symbol;

	scope = list_remove_first(&table->scopes);

	for (symbol = list_first(&scope->defined_symbols); symbol;
		symbol = list_next(&symbol->list_node)) {

		/* TODO free this memory (but I didn't allocate it, rats) */
		symbol_pop_definition(table, symbol);
	}
}


struct symdef *symbol_push_definition(struct symtab *table,
	struct symbol *symbol, struct symdef *symdef)
{
	struct symtab_scope *scope;

	scope = list_first(&table->scopes);
	list_insert_last(&scope->defined_symbols, &symbol->list_node);

	return symbol->def = (struct symdef *)
		list_insert_first(&symbol->defs, &symdef->list_node);
}


struct symdef *symbol_pop_definition(struct symtab *table, struct symbol *symbol)
{
	assert(list_first(&symbol->defs) != &undef_symbol_definition);

	struct symtab_scope *scope;

	scope = list_first(&table->scopes);
	list_remove(&scope->defined_symbols, &symbol->list_node);

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
