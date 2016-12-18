#ifndef INBUF_H
#define INBUF_H

#include "error.h"
#include <stdio.h>

#define INBUF_EOF	(-1)

struct inbuf {
	FILE *file;	/* file managed by this buffer */
	char *data;	/* buffered data */
	size_t size;	/* size of the buffer */
	size_t count;	/* number of bytes in the buffer */
	size_t offset;	/* read offset in the buffer */
};

mcc_error_t inbuf_open(struct inbuf *inbuf, size_t size, const char *filename);
mcc_error_t inbuf_open_mem(struct inbuf *inbuf, char *string, size_t len);
void inbuf_close(struct inbuf *inbuf);

int inbuf_get_char(struct inbuf *inbuf);

#endif
