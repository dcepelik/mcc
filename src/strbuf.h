/*
 * strbuf:
 * Growing string buffer.
 */

#ifndef STRBUF_H
#define STRBUF_H

#include <stdlib.h>
#include <stdbool.h>
#include "mempool.h"

struct strbuf
{
	char *str;		/* string being built */
	size_t len;		/* length of the string */
	size_t size;		/* current size of the buffer */
};

bool strbuf_init(struct strbuf *buf, size_t init_size);
bool strbuf_putc(struct strbuf *buf, char c);
bool strbuf_prepare_write(struct strbuf *buf, size_t count);
char *strbuf_get_string(struct strbuf *buf);
size_t strbuf_strlen(struct strbuf *buf);
char *strbuf_copy_to_mempool(struct strbuf *buf, struct mempool *pool);
void strbuf_reset(struct strbuf *buf);
void strbuf_free(struct strbuf *buf);

#endif
