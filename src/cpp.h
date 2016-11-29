#ifndef CPP_H
#define CPP_H

enum cpp_directive
{
	CPP_DIRECTIVE_IF,
	CPP_DIRECTIVE_IFDEF,
	CPP_DIRECTIVE_IFNDEF,
	CPP_DIRECTIVE_ELIF,
	CPP_DIRECTIVE_ELSE,
	CPP_DIRECTIVE_ENDIF,
	CPP_DIRECTIVE_INCLUDE,
	CPP_DIRECTIVE_DEFINE,
	CPP_DIRECTIVE_UNDEF,
	CPP_DIRECTIVE_LINE,
	CPP_DIRECTIVE_ERROR,
	CPP_DIRECTIVE_PRAGMA,
};

#include "cppfile.h"

struct cpp_if
{
	enum cpp_directive directive;	/* if/ifdef/ifndef? */
	/* TODO add location info */
};

struct cppfile;

struct tokinfo *cpp_next(struct cppfile *file);
void cpp_setup_symtab(struct cppfile *file);

#endif
