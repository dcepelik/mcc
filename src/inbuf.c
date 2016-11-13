#include "inbuf.h"
#include "debug.h"
#include <assert.h>
#include <stdlib.h>


static void inbuf_fill(struct inbuf *buf)
{
	buf->count = fread(buf->data, sizeof(char), buf->size, buf->file);
	buf->offset = 0;
}


mcc_error_t inbuf_open(struct inbuf *buf, size_t size, const char *filename)
{
	buf->file = fopen(filename, "r");
	if (!buf->file)
		return MCC_ERROR_ACCESS;

	buf->size = size;
	buf->count = 0;
	buf->offset = 0;

	buf->data = malloc(buf->size);
	if (!buf->data) {
		fclose(buf->file);
		return MCC_ERROR_NOMEM;
	}

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
