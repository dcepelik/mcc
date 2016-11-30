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
	SYMBOL_TYPE_UNDEF,		/* not a defined symbol */
	SYMBOL_TYPE_CPP_MACRO,		/* C preprocessor macro */
	SYMBOL_TYPE_CPP_BUILTIN,	/* C preprocessor's built-in macro */
	SYMBOL_TYPE_CPP_DIRECTIVE	/* C preprocessor directive */
};

struct symbol
{
	struct hashnode hashnode;
	enum symbol_type type;

	union {
		enum cpp_directive directive;
	};
};

struct symtab
{
	struct objpool symbol_pool;	/* object pool for the symbols */
	struct hashtab table;		/* a hash table */
};

bool symtab_init(struct symtab *table);
bool symtab_contains(struct symtab *table, const char *name);
struct symbol *symtab_search(struct symtab *table, const char *name);
struct symbol *symtab_insert(struct symtab *table, const char *name);
void symtab_free(struct symtab *table);

const char *symbol_get_name(struct symbol *symbol);
const char *symbol_get_type(struct symbol *symbol);

#endif
