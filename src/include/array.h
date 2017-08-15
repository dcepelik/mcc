#ifndef ARRAY_H
#define ARRAY_H

#include <stdlib.h>

#define array_push(arr, item)	do { \
	size_t __len; \
	arr = array_claim(arr, 1); \
	__len = array_size(arr); \
	arr[__len - 1] = item; \
} while (0)

void *array_new(size_t num_items, size_t item_size);
void *array_claim(void *arr, size_t num_items);
size_t array_size(void *arr);
void array_reset(void *arr);
void array_delete(void *arr);

#endif
