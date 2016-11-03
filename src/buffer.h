#ifndef BUFFER_H
#define BUFFER_H

#include "error.h"
#include <stdio.h>

struct buffer {
	FILE *file;	/* file managed by this buffer */
	char *data;	/* buffered data */
	size_t size;	/* size of the buffer */
	size_t count;	/* number of bytes in the buffer */
	size_t offset;	/* read offset in the buffer */
};

mcc_error_t buffer_open(struct buffer *buffer, size_t size, char *filename);
int buffer_get_char(struct buffer *buffer);
void buffer_close(struct buffer *buffer);

#endif
