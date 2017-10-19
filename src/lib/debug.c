#include "debug.h"
#include <string.h>

bool shall_debug_file(const char *filename)
{
	if (strcmp(filename, "src/cpp-files.c") == 0)
		return false;

	if (strcmp(filename, "src/cpp-directives.c") == 0)
		return true;

	if (strcmp(filename, "src/cpp.c") == 0)
		return true;

	return false;
}
