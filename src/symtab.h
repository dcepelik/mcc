/*
 * symtab:
 * Symbol table.
 */

#ifndef SYMBOL_H
#define SYMBOL_H

#include "cpp.h"
#include "hashtab.h"
#include "objpool.h"
#include <stdbool.h>
#include <stdlib.h>

enum symbol_type
{
	SYMBOL_TYPE_UNDEF,		/* symbol has no proper definition */
	SYMBOL_TYPE_CPP_MACRO,		/* C preprocessor macro */
	SYMBOL_TYPE_CPP_BUILTIN,	/* C preprocessor's built-in macro */
	SYMBOL_TYPE_CPP_DIRECTIVE	/* C preprocessor directive */
};

struct symdef
{
	struct list_node scope_list_node;
	struct list_node def_stack_node;
	struct symbol *symbol;
	enum symbol_type type;
	union {
		enum cpp_directive directive;
		struct macro *macro;
	};
};

struct symbol
{
	struct hashnode hashnode;	/* allows hashing */
	struct list defs;		/* definition stack */
	struct symdef *def;		/* shortcut for list_first(defs) */
};

struct symtab
{
	struct objpool symbol_pool;	/* objpool for the symbols */
	struct objpool scope_pool;	/* objpool for the scopes */
	struct hashtab table;		/* a hash table */
	struct list scopes;		/* list of active scopes */
};

bool symtab_init(struct symtab *table);
void symtab_free(struct symtab *table);

bool symtab_contains(struct symtab *table, char *name);
struct symbol *symtab_search(struct symtab *table, char *name);
struct symbol *symtab_insert(struct symtab *table, char *name);

void symtab_scope_begin(struct symtab *table);
void symtab_scope_end(struct symtab *table);

char *symbol_get_name(struct symbol *symbol);
const char *symbol_get_type(struct symbol *symbol);
struct symdef *symbol_push_definition(struct symtab *table,
	struct symbol *symbol, struct symdef *symdef);
struct symdef *symbol_pop_definition(struct symtab *table, struct symbol *symbol);

#endif
