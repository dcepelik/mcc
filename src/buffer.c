#include "buffer.h"
#include "debug.h"
#include <assert.h>
#include <stdlib.h>


static void buffer_fill(struct buffer *buffer)
{
	DEBUG_MSG("Filling buffer");

	buffer->count = fread(buffer->data, sizeof(char), buffer->size, buffer->file);
	buffer->offset = 0;
}


mcc_error_t buffer_open(struct buffer *buffer, size_t size, char *filename)
{
	buffer->file = fopen(filename, "r");
	if (!buffer->file)
		return MCC_ERROR_ACCESS;

	buffer->size = size;
	buffer->count = 0;
	buffer->offset = 0;

	buffer->data = malloc(buffer->size * sizeof(char));
	if (!buffer->data) {
		fclose(buffer->file);
		return MCC_ERROR_NOMEM;
	}

	return MCC_ERROR_OK;
}


int buffer_get_char(struct buffer *buffer)
{		
	if (buffer->offset == buffer->count)
		buffer_fill(buffer);

	if (buffer->count == 0)
		return -1;

	return buffer->data[buffer->offset++];
}


void buffer_close(struct buffer *buffer)
{
	assert(buffer->file != NULL);

	fclose(buffer->file);
}
