#include "array.h"
#include <stdlib.h>


int main(void)
{
	int *arr;
	size_t i;

	arr = array_new(16, sizeof(int));

	for (i = 0; i < 2048; i++) {
		arr = array_claim(arr, 1);
		arr[i] = i;
	}

	for (i = 0; i < array_size(arr); i++)
		printf("%i\n", arr[i]);

	array_delete(arr);

	return EXIT_SUCCESS;
}
