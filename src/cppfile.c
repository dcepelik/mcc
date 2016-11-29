#include "cppfile.h"




struct cppfile *cppfile_new(const char *filename)
{
	struct cppfile *file = malloc(sizeof(*file));
	if (!file)
		return NULL;

	
