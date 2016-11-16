#include "strbuf.h"
#include <string.h>
#include <assert.h>


#define MAX(a, b) ((a) >= (b) ? (a) : (b))


static bool strbuf_resize(struct strbuf *buf, size_t new_size)
{
	assert(new_size > 0);

	buf->str = realloc(buf->str, new_size);
	if (!buf->str)
		return false;

	buf->size = new_size;
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
