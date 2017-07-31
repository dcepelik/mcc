#ifndef ARRAY_H
#define ARRAY_H

#include <stdlib.h>

void *array_new(size_t num_items, size_t item_size);
void *array_claim(void *arr, size_t num_items);
size_t array_size(void *arr);
void array_reset(void *arr);
void array_delete(void *arr);

#endif
