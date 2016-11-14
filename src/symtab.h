#ifndef SYMBOL_H
#define SYMBOL_H

#include "error.h"
#include "mempool.h"
#include "lexer.h"
#include <stdbool.h>
#include <stdlib.h>

enum symbol_type
{
	SYMBOL_TYPE_CPP_MACRO,		/* C preprocessor macro */
};

struct symbol
{
	char *name;			/* symbol name */
	enum symbol_type type;		/* symbol type */
	struct symbol *next;		/* next symbol in the list */
};

struct symtab
{
	struct mempool symbols;		/* mempool for the symbols */
	struct symbol **table;		/* the table itself */
	size_t count;			/* number of items in the table */
	size_t size;			/* size (capacity) of the table */
};

mcc_error_t symtab_init(struct symtab *table);
bool symtab_contains(struct symtab *table, const char *name);
struct symbol *symtab_search(struct symtab *table, const char *name);
struct symbol *symtab_insert(struct symtab *table, const char *name);
void symtab_free(struct symtab *table);

#endif
