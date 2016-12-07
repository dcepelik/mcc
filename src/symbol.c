#include "debug.h"
#include "mempool.h"
#include "symbol.h"
#include "macro.h"
#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>


struct symbol *symtab_search(struct symtab *symtab, char *name)
{
	return hashtab_search(&symtab->table, name);
}


bool symtab_contains(struct symtab *symtab, char *name)
{
	return hashtab_contains(&symtab->table, name);
}


/*
 * Artificial symbol definition. It's the definition that every symbol->def
 * points to prior to being defined properly. It's never put onto the
 * definition stack. It simplifies conditionals (it's not an error to ask for
 * the type of an undefined symbol).
 */
static struct symdef symbol_undef = {
	.type = SYMBOL_TYPE_UNDEF
};


static inline void scope_init(struct scope *scope)
{
	list_init(&scope->defs);
}


static inline struct scope *scope_new(struct symtab *table)
{
	struct scope *scope;

	scope = objpool_alloc(&table->scope_pool);
	if (scope)
		scope_init(scope);

	return scope;
}


static inline void scope_free(struct scope *scope)
{
	list_free(&scope->defs);
}


static inline void scope_delete(struct symtab *table, struct scope *scope)
{
	scope_free(scope);
	objpool_dealloc(&table->scope_pool, scope);
}


bool symtab_init(struct symtab *table)
{
	objpool_init(&table->symbol_pool, sizeof(struct symbol), 256);
	objpool_init(&table->scope_pool, sizeof(struct scope), 8);
	objpool_init(&table->symdef_pool, sizeof(struct symdef), 256);
	list_init(&table->scope_stack);

	scope_init(&table->file_scope);
	list_insert_first(&table->scope_stack, &table->file_scope.scope_stack_node);

	return hashtab_init(&table->table, &table->symbol_pool, 256);
}


void symtab_free(struct symtab *table)
{
	objpool_free(&table->symbol_pool);
	objpool_free(&table->scope_pool);
	objpool_free(&table->symdef_pool);
	list_free(&table->scope_stack);
	scope_free(&table->file_scope);
	hashtab_free(&table->table);
}


static inline struct symbol *symbol_new(struct symtab *table)
{
	struct symbol *symbol;

	symbol = objpool_alloc(&table->symbol_pool);
	if (symbol) {
		list_init(&symbol->def_stack);
		symbol->def = &symbol_undef;
	}

	return symbol;
}


static void symbol_delete(struct symtab *table, struct symbol *symbol)
{
	list_free(&symbol->def_stack);
	objpool_dealloc(&table->symbol_pool, symbol);
}


struct symdef *symbol_define(struct symtab *table, struct symbol *symbol)
{
	struct symdef *def;
	struct scope *scope;

	scope = list_first(&table->scope_stack);
	assert(scope != NULL);

	def = objpool_alloc(&table->symdef_pool);
	if (!def)
		return NULL;

	def->symbol = symbol;
	list_insert_last(&scope->defs, &def->scope_list_node);
	list_insert_first(&symbol->def_stack, &def->def_stack_node);

	return symbol->def = def;
}


char *symbol_get_name(struct symbol *symbol)
{
	assert(symbol);

	return symbol->hashnode.key;
}


const char *symbol_type_to_string(enum symbol_type type)
{
	switch (type)
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


struct symbol *symtab_insert(struct symtab *table, char *name)
{
	struct symbol *symbol;
	
	symbol = symbol_new(table);
	if (symbol)
		hashtab_insert(&table->table, name, &symbol->hashnode);

	return symbol;
}


bool symtab_scope_begin(struct symtab *table)
{
	struct scope *scope;

	scope = scope_new(table);
	if (!scope)
		return false;

	list_insert_first(&table->scope_stack, &scope->scope_stack_node);
	return true;
}


void symtab_scope_end(struct symtab *table)
{
	assert(list_first(&table->scope_stack) != &table->file_scope.scope_stack_node);

	struct symbol *symbol;
	struct scope *scope;
	
	scope = list_first(&table->scope_stack);

	list_foreach(struct symdef, def, &scope->defs, scope_list_node) {
		symbol = def->symbol;

		assert(list_first(&symbol->def_stack) == &def->def_stack_node);
		list_remove_first(&symbol->def_stack);

		symbol->def = container_of(list_first(&symbol->def_stack), struct symdef, def_stack_node);
		if (!symbol->def)
			symbol->def = &symbol_undef;

		//objpool_dealloc(&table->symdef_pool, def); TODO How?
	}

	list_remove_first(&table->scope_stack);

	scope_delete(table, scope);
}


void symtab_dump(struct symtab *table, FILE *fout)
{
	size_t i;
	
	i = list_length(&table->scope_stack) - 1;

	fprintf(fout, "Symbol table:\n");
	list_foreach(struct scope, scope, &table->scope_stack, scope_stack_node) {
		fprintf(fout, "\tScope %lu:\n", i);

		list_foreach(struct symdef, def, &scope->defs, scope_list_node) {
			fprintf(fout, "\t\t%-16s\t%-16s",
				symbol_get_name(def->symbol),
				symbol_type_to_string(def->type));

			if (def->type == SYMBOL_TYPE_CPP_MACRO) {
				cpp_dump_toklist(&def->macro->expansion);
			}
			else {
				fputc('\n', fout);
			}
		}

		i--;
	}
}