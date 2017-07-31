#ifndef UTF8_H
#define UTF8_H

#include "strbuf.h"
#include <inttypes.h>

/* TODO typedef uint8_t utf8_t; */
typedef char utf8_t;

void utf8_from_wchar(wchar_t wc, utf8_t *bytes);

#endif
