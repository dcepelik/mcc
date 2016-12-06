#include "debug.h"
#include "mempool.h"
#include "symtab.h"
#include "macro.h"
#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>


/*
* Artificial symbol definition. It's the definition that every symbol->def
* points to prior to being defined properly. It's never put onto the
* definition stack. It simplifies conditionals.
*/
static struct symdef symbol_undef = {
	.type = SYMBOL_TYPE_UNDEF
};

struct symtab_scope
{
	struct list_node scope_stack_node;
	struct list defs;	/* symbol definitions in this scope */
};


/*
 * Default scope to be kept at the bottom of the scope stack. It's
 * the root of the scope tree.
 */
static struct symtab_scope global_scope;


static inline void symtab_scope_init(struct symtab_scope *scope)
{
	list_init(&scope->defs);
}


static inline void symtab_scope_free(struct symtab_scope *scope)
{
	list_free(&scope->defs);
}


bool symtab_init(struct symtab *symtab)
{
	objpool_init(&symtab->symbol_pool, sizeof(struct symbol), 256);
	objpool_init(&symtab->scope_pool, sizeof(struct symtab_scope), 64);
	list_init(&symtab->scopes);

	symtab_scope_init(&global_scope);
	list_insert_first(&symtab->scopes, &global_scope.scope_stack_node);

	return hashtab_init(&symtab->table, &symtab->symbol_pool, 256);
}


void symtab_free(struct symtab *symtab)
{
	/*
	struct symbol *ahoj = symtab_search(symtab, "ahoj");
	assert(ahoj);
	assert(ahoj->def);
	assert(ahoj->def->type == SYMBOL_TYPE_UNDEF);

	struct list_node *n;
	for (n = list_first(&global_scope.defined_symbols);
		n; n = list_next(n)) {
		struct symbol *s = container_of(n, struct symbol, list_node);
		printf("%s\n", symbol_get_name(s));
	}
	*/
	hashtab_free(&symtab->table);
	objpool_free(&symtab->symbol_pool);
	objpool_free(&symtab->scope_pool);
}


struct symbol *symtab_search(struct symtab *symtab, char *name)
{
	return hashtab_search(&symtab->table, name);
}


bool symtab_contains(struct symtab *symtab, char *name)
{
	return hashtab_contains(&symtab->table, name);
}


static void symbol_init(struct symbol *symbol)
{
	list_init(&symbol->defs);
	symbol->def = &symbol_undef;
}


struct symbol *symtab_insert(struct symtab *symtab, char *name)
{
	struct symbol *symbol;
	
	symbol = hashtab_insert(&symtab->table, name);
	symbol_init(symbol);

	//if (strcmp(name, "ahoj") == 0)
		//symtab_dump(symtab);

	return symbol;
}


char *symbol_get_name(struct symbol *symbol)
{
	return symbol->hashnode.key;
}


void symtab_scope_begin(struct symtab *table)
{
	//DEBUG_MSG("Scope begun");

	struct symtab_scope *scope = objpool_alloc(&table->scope_pool);
	symtab_scope_init(scope);
	list_insert_first(&table->scopes, &scope->scope_stack_node);
}


void symtab_scope_end(struct symtab *table)
{
	assert(list_first(&table->scopes) != &global_scope);

	struct symbol *symbol;
	struct symtab_scope *scope = list_first(&table->scopes);

	list_foreach(struct symdef, def, &scope->defs, scope_list_node) {
		symbol = def->symbol;
		list_remove(&symbol->defs, &def->def_stack_node);

		symbol->def = container_of(list_first(&symbol->defs), struct symdef, def_stack_node);
		if (!symbol->def)
			symbol->def = &symbol_undef;

		/* TODO free def */
	}

	list_remove_first(&table->scopes);

	symtab_scope_free(scope);
	objpool_dealloc(&table->scope_pool, scope);

	//DEBUG_MSG("Scope ended");
}


struct symdef *symbol_push_definition(struct symtab *table,
	struct symbol *symbol, struct symdef *symdef)
{
	struct symtab_scope *scope;

	symdef->symbol = symbol;
	scope = list_first(&table->scopes);
	list_insert_last(&scope->defs, &symdef->scope_list_node);

	list_insert_first(&symbol->defs, &symdef->def_stack_node);

	return symbol->def = symdef;
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
	}

	return NULL;
}


void symtab_dump(struct symtab *table)
{
	size_t i = 0;

	printf("Symbol table:\n");
	list_foreach(struct symtab_scope, scope, &table->scopes, scope_stack_node) {
		printf("Scope %lu:\n", i);

		list_foreach(struct symdef, def, &scope->defs, scope_list_node) {
			printf(
				"\t%-16s\t%-16s\t%s",
				symbol_get_name(def->symbol),
				symdef_get_type(def),
				""
			);

			if (def->type == SYMBOL_TYPE_CPP_MACRO) {
				cpp_dump_toklist(&def->macro->expansion);
			}
			else {
				putchar('\n');
			}
		}

		i++;
	}

	printf("---\n");
}
