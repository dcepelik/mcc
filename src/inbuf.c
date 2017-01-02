#include "common.h"
#include "inbuf.h"
#include "debug.h"
#include <assert.h>
#include <stdlib.h>


static void inbuf_fill(struct inbuf *buf)
{
	buf->count = fread(buf->data, sizeof(char), buf->size, buf->file);
	buf->offset = 0;
}


static void inbuf_open_internal(struct inbuf *buf, size_t size, FILE *file)
{
	assert(size > 0);

	buf->file = file;
	buf->size = size;
	buf->count = 0;
	buf->offset = 0;

	buf->data = mcc_malloc(buf->size);
}


mcc_error_t inbuf_open(struct inbuf *buf, size_t size, const char *filename)
{
	FILE *file;
	
	file = fopen(filename, "r");
	if (!file)
		return MCC_ERROR_ACCESS; /* TODO Error reporting */

	inbuf_open_internal(buf, size, file);
	return MCC_ERROR_OK;
}


mcc_error_t inbuf_open_mem(struct inbuf *buf, char *str, size_t len)
{
	FILE *file;
	
	file = fmemopen(str, len, "r");
	if (!file)
		return MCC_ERROR_ACCESS; /* TODO Error reporting */

	inbuf_open_internal(buf, len, file);
	return MCC_ERROR_OK;
}


int inbuf_get_char(struct inbuf *buf)
{		
	if (buf->offset == buf->count)
		inbuf_fill(buf);

	if (buf->count == 0)
		return INBUF_EOF;

	return buf->data[buf->offset++];
}


void inbuf_close(struct inbuf *buf)
{
	assert(buf->file != NULL);

	free(buf->data);
	fclose(buf->file);
}
