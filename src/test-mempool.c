#include "mempool.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "debug.h"


const char *strings[] = {
	"hello ",
	"world, ",
	"how ",
	"are ",
	"you?"
};

int main(void)
{
	struct mempool pool;
	size_t i;
	char *str;

	mempool_init(&pool, 10);

	for (i = 0; i < sizeof(strings) / sizeof(*strings); i++) {
		str = mempool_alloc(&pool, strlen(strings[i]) + 1);
		DEBUG_PRINTF("Writing at 0x%x", str);
		*str = *strings[i];
	}

	mempool_free(&pool);
}
