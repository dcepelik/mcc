#ifndef MACRO_H
#define MACRO_H

#include "cppfile.h"
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
};

void macro_init(struct macro *macro);
void macro_free(struct macro *macro);

void macro_expand(struct cppfile *file, struct list *invocation,
	struct list *expansion);

void macro_dump(struct macro *macro);

#endif
