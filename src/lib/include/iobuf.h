#ifndef IOBUF_H
#define IOBUF_H

/*
 * - FILE
 * - file descriptor
 * - memory
 */

#include "common.h"

enum iobuf_mode
{
	IOBUF_READ = 1 << 0,
	IOBUF_WRITE = 1 << 1,
};

struct iobuf_ops
{
	void (*read)(struct iobuf *buf);
	void (*write)(struct iobuf *buf);
	void (*dispose)(struct iobuf *buf);
};

/*
 * Abstract I/O buffer: something that can be read from or written to or both.
 */
struct iobuf
{
	struct iobuf_ops ops;
	enum iobuf_mode mode;
};

struct iobuf iobuf_stdin;
struct iobuf iobuf_stdout;
struct iobuf iobuf_stderr;

void iobuf_init_fd(struct iobuf *buf, int fd);
void iobuf_init_file(struct iobuf *buf, FILE *f);
void iobuf_init_memory(struct iobuf *buf, void *mem, size_t size);

void iobuf_close(struct iobuf *buf);

ssize_t iobuf_read(struct iobuf *buf, byte_t *dst, size_t count);
ssize_t iobuf_write(struct iobuf *buf, byte_t *src, size_t count);

#endif
