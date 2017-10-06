#include "debug.h"
#include <string.h>

bool shall_debug_file(const char *filename)
{
	if (strcmp(filename, "cpp.c") == 0)
		return true;

	if (strcmp(filename, "cppfile.c") == 0)
		return true;

	if (strcmp(filename, "cpp.h") == 0)
		return true;

	if (strcmp(filename, "lexer.c") == 0)
		return true;

	if (strcmp(filename, "list.c") == 0)
		return true;

	if (strcmp(filename, "symbol.c") == 0)
		return true;

	if (strcmp(filename, "macro.c") == 0)
		return true;

	if (strcmp(filename, "cpp-files.c") == 0)
		return true;

	if (strcmp(filename, "cpp-directives.c") == 0)
		return true;

	if (strcmp(filename, "src/parse-expr.c") == 0)
		return true;

	if (strcmp(filename, "src/parse-decl.c") == 0)
		return true;

	return false;
}
