/*
 * symtab:
 * Symbol table.
 */

#ifndef SYMBOL_H
#define SYMBOL_H

#include "cpp-internal.h"
#include "hashtab.h"
#include "keyword.h"
#include "objpool.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct scope
{
	struct list_node scope_stack_node;
	struct list defs;	/* symbol definitions in this scope */
};

struct symtab
{
	struct objpool symbol_pool;	/* pool for the symbols */
	struct objpool scope_pool;	/* pool for the scopes */
	struct objpool symdef_pool;	/* pool for symbol definitions */
	struct hashtab table;		/* hash table for the symbols */
	struct list scope_stack;	/* stack of active scopes */
	struct scope file_scope;	/* file (global) scope */
};

void symtab_init(struct symtab *table);
void symtab_free(struct symtab *table);

struct symbol *symtab_search(struct symtab *symtab, char *name);
bool symtab_contains(struct symtab *symtab, char *name);
struct symbol *symtab_insert(struct symtab *table, char *name);
struct symbol *symtab_search_or_insert(struct symtab *table, char *name);

void symtab_scope_begin(struct symtab *table);
void symtab_scope_end(struct symtab *table);

void symtab_dump(struct symtab *table, FILE *fout);

enum symbol_type
{
	SYMBOL_TYPE_CPP_DIRECTIVE,	/* C preprocessor directive */
	SYMBOL_TYPE_CPP_MACRO,		/* C preprocessor macro */
	SYMBOL_TYPE_CPP_MACRO_ARG,	/* C preprocessor's macro argument */
	SYMBOL_TYPE_C_KEYWORD,		/* C language keyword */
	SYMBOL_TYPE_UNDEF		/* symbol has no proper definition */
};

const char *symbol_type_to_string(enum symbol_type type);

struct symbol
{
	struct hashnode hashnode;	/* allows symbols to be hashed */
	struct list def_stack;		/* definition stack */
	struct symdef *def;		/* shortcut for list_first(defs) */
};

char *symbol_get_name(struct symbol *symbol);
struct symdef *symbol_define(struct symtab *table, struct symbol *symbol);

struct symdef
{
	struct list_node scope_list_node;
	struct list_node def_stack_node;
	struct symbol *symbol;
	enum symbol_type type;
	union {
		enum cpp_directive directive;	/* C preprocessor directive */
		const struct kwdinfo *kwdinfo;	/* C keyword */
		struct macro macro;		/* C preprocessor macro */
		struct macro_arg macro_arg;	/* argument of a C preprocessor macro */
	};
};

#endif
