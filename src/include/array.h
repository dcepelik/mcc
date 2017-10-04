#ifndef ARRAY_H
#define ARRAY_H

#include <assert.h>
#include <stdlib.h>

struct array_header
{
	size_t item_size;
	size_t num_items;
	size_t capacity;
};

#define array_last(arr)		(arr)[array_size(arr) - 1]
#define array_push(arr, item)	((arr) = array_claim((arr), 1), (arr)[array_size(arr) - 1] = item)
#define array_pop(arr)		(assert(array_get_header(arr)->num_items > 0), \
					(arr)[--array_get_header(arr)->num_items])

#define array_push_new(arr)	((arr) = array_claim((arr), 1), &arr[array_size(arr) - 1])

static inline struct array_header *array_get_header(void *arr)
{
	return (struct array_header *)((unsigned char *)arr - sizeof(struct array_header));
}

void *array_new(size_t num_items, size_t item_size);
void *array_push_helper(void **arr);
void *array_claim(void *arr, size_t num_items);
size_t array_size(void *arr);
void array_reset(void *arr);
void array_delete(void *arr);

#endif
