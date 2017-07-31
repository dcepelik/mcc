/*
 * UTF-8 character string literals.
 */

#ifndef LSTR_H
#define LSTR_H

#include "utf8.h"

/*
 * String literal. @len bytes long stream @str of UTF-8 bytes, which
 * may contain embedded NULs.
 *
 * See 6.4.5 String literals.
 */
struct lstr
{
	utf8_t *str;			/* the string itself */
	size_t len;			/* length of the literal (in bytes) */
};

#endif
