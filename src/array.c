#include "common.h"
#include "array.h"
#include "debug.h"
#include <assert.h>

extern struct array_header *array_get_header(void *arr);

static void *array_resize(void *arr, size_t new_capacity, size_t item_size)
{
	DEBUG_PRINTF("Resizing array to %lu items", new_capacity);

	size_t actual_size;
	struct array_header *header = NULL;

	if (arr)
		header = array_get_header(arr);

	actual_size = sizeof(struct array_header) + new_capacity * item_size;

	header = mcc_realloc(header, actual_size);
	header->item_size = item_size;
	header->capacity = new_capacity; 

	return header + 1;

}

void *array_new(size_t init_capacity, size_t item_size)
{
	void *arr = array_resize(NULL, init_capacity, item_size);
	array_get_header(arr)->num_items = 0;
	
	return arr;
}

void *array_claim(void *arr, size_t num_items)
{
	struct array_header *header;

	header = array_get_header(arr);
	if (header->num_items + num_items > header->capacity) {
		arr = array_resize(arr, 2 * header->capacity, header->item_size);
		header = array_get_header(arr);
	}

	header->num_items += num_items;

	return arr;
}

size_t array_size(void *arr)
{
	return array_get_header(arr)->num_items;
}

void array_reset(void *arr)
{
	array_get_header(arr)->num_items = 0;
}

void array_delete(void *arr)
{
	free(array_get_header(arr));
}

void *array_push_helper(void **arr)
{
	*arr = array_claim(*arr, 1);
	return &arr[array_size(arr) - 1];
}
