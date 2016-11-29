#include "strbuf.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>


#define MAX(a, b) ((a) >= (b) ? (a) : (b))


static bool strbuf_resize(struct strbuf *buf, size_t new_size)
{
	assert(new_size > 0);

	buf->str = realloc(buf->str, new_size);
	if (!buf->str)
		return false;

	buf->size = new_size;
	if (buf->len > buf->size - 1)
		buf->len = buf->size - 1;

	return true;
}


bool strbuf_init(struct strbuf *buf, size_t init_size)
{
	assert(init_size > 0);

	buf->str = NULL;
	buf->len = 0;
	buf->size = 0;

	return strbuf_resize(buf, init_size);
}


bool strbuf_prepare_write(struct strbuf *buf, size_t count)
{
	if (buf->len + count >= buf->size) /* >= because of the '\0' */
		return strbuf_resize(buf, MAX(count, 2 * buf->size));

	return true;
}


bool strbuf_putc(struct strbuf *buf, char c)
{
	if (!strbuf_prepare_write(buf, 1))
		return false;

	buf->str[buf->len++] = c;
	return true;
}


char *strbuf_get_string(struct strbuf *buf)
{
	buf->str[buf->len] = '\0'; /* there's room */
	return buf->str;
}


size_t strbuf_strlen(struct strbuf *buf)
{
	return buf->len;
}


char *strbuf_copy_to_mempool(struct strbuf *buf, struct mempool *pool)
{
	char *str;

	str = mempool_alloc(pool, buf->len + 1);
	if (!str)
		return NULL;

	memcpy(str, buf->str, buf->len + 1);
	str[buf->len] = '\0';
	return str;
}


void strbuf_reset(struct strbuf *buf)
{
	buf->len = 0;
}


void strbuf_free(struct strbuf *buf)
{
	free(buf->str);
}


size_t strbuf_vprintf_at(struct strbuf *buf, size_t offset, char *fmt, va_list args)
{
	va_list args2;
	size_t num_written;
	size_t extra_size_needed;

	va_copy(args2, args);

	num_written = vsnprintf(NULL, 0, fmt, args2);
	if (num_written < 0)
		return -1;

	extra_size_needed = buf->size - offset - (num_written + 1);
	if (extra_size_needed > 0) {
		strbuf_prepare_write(buf, extra_size_needed);
		buf->len = MAX(buf->len, offset + num_written);
	}

	vsnprintf(buf->str + offset, (num_written + 1), fmt, args);

	va_end(args2);
	va_end(args);

	return num_written;
}


size_t strbuf_printf(struct strbuf *buf, char *fmt, ...)
{
	size_t num_written;

	va_list args;
	va_start(args, fmt);
	num_written = strbuf_vprintf_at(buf, buf->len, fmt, args);
	va_end(args);

	return num_written;
}
