/*
 * Macro expansion routines.
 *
 * TODO Refactor the code.
 * TODO Remove the recursion from the expansion if possible.
 */

#ifndef MACRO_H
#define MACRO_H

/*
 * Note on terminology used:
 *
 * 3.3:  argument: "... a sequence of preprocessing tokens in the
 *       comma-separated list bounded by the parentheses in a function-like
 *       macro invocation"
 *
 * 3.16: parameter: "... an identifer from the comma-separated list bounded
 *       by the parentheses immediately following the macro name in
 *       a function-like macro defnition"
 */

#include "list.h"

enum macro_type
{
	MACRO_TYPE_OBJLIKE,	/* an object-like macro */
	MACRO_TYPE_FUNCLIKE,	/* a function-like macro */
};

struct macro
{
	char *name;		/* name of the macro */
	struct list args;	/* argument list */
	struct list expansion;	/* expansion list */
	enum macro_type type;	/* type of the macro */
	bool is_macro_arg;	/* is this macro is another macro's argument? */
	bool is_expanding;	/* is this macro just expanding? */
};

struct macro_arg
{
	struct list tokens;	/* tokens that constitute this argument */
	struct list expansion;	/* complete macro expansion of @tokens */
};

struct cpp;

void macro_init(struct macro *macro);
void macro_free(struct macro *macro);

void macro_expand(struct cpp *file, struct list *invocation,
	struct list *expansion);

void macro_dump(struct macro *macro);

#endif
