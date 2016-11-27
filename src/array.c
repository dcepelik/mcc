#include "array.h"
#include "debug.h"


struct array_header
{
	size_t item_size;
	size_t num_items;
	size_t capacity;
};


static inline struct array_header *array_get_header(void *arr)
{
	return (struct array_header *)((unsigned char *)arr - sizeof(struct array_header));
}


static void *array_resize(void *arr, size_t new_capacity, size_t item_size)
{
	DEBUG_PRINTF("Resizing array to %lu items", new_capacity);

	size_t actual_size;
	struct array_header *header = NULL;

	if (arr)
		header = array_get_header(arr);

	actual_size = sizeof(struct array_header) + new_capacity * item_size;

	header = realloc(header, actual_size);
	if (!header)
		return NULL;

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
		if (!arr)
			return NULL;

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