#include "debug.h"
#include <string.h>


bool shall_debug_file(const char *filename)
{
	if (strcmp(filename, "cpp.c") == 0)
		return true;

	if (strcmp(filename, "cpp.h") == 0)
		return true;

	return false;
}
