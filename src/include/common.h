#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>

#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof(*(arr)))

void *mcc_malloc(size_t size);
void *mcc_realloc(void *ptr, size_t size);

#endif
