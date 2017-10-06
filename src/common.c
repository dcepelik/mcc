#include "common.h"
#include <stdio.h>

void *mcc_realloc(void *ptr, size_t new_size)
{
	void *x = realloc(ptr, new_size);
	if (!x) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	return x;
}

void *mcc_malloc(size_t size)
{
	return mcc_realloc(NULL, size);
}
