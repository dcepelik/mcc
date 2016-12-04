#ifndef CPP_H
#define CPP_H

#include "list.h"

enum cpp_directive
{
	CPP_DIRECTIVE_DEFINE,
	CPP_DIRECTIVE_ELIF,
	CPP_DIRECTIVE_ELSE,
	CPP_DIRECTIVE_ENDIF,
	CPP_DIRECTIVE_ERROR,
	CPP_DIRECTIVE_IF,
	CPP_DIRECTIVE_IFDEF,
	CPP_DIRECTIVE_IFNDEF,
	CPP_DIRECTIVE_INCLUDE,
	CPP_DIRECTIVE_LINE,
	CPP_DIRECTIVE_PRAGMA,
	CPP_DIRECTIVE_UNDEF,
};

struct cpp_if
{
	struct list_node list_node;
	struct tokinfo *tokinfo;
	bool skip_this_branch;
	bool skip_next_branch;
};

#include "cppfile.h"

struct cppfile;

struct tokinfo *cpp_next(struct cppfile *file);
void cpp_setup_symtab(struct cppfile *file);

#endif
