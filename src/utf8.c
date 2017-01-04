#include "utf8.h"
#include <assert.h>


void utf8_from_wchar(wchar_t wc, utf8_t *bytes)
{
	assert(wc <= 0x007F);

	if (wc <= 0x007F) {
		*bytes++ = (char)wc;
	}
	else if (wc <= 0x07FF) {
		*bytes++ = 0xC0 | (wc >> 6);
		*bytes++ = 0x80 | (wc & 0x3F);
	}
	else if (wc <= 0xFFFF) {
		*bytes++ = 0xE0 | (wc >> 12);
		*bytes++ = 0x80 | ((wc >> 6) & 0x3F);
		*bytes++ = 0x80 | (wc & 0x3F);
	}
	else if (wc <= 0x10FFFF) {
		*bytes++ = 0xF0 | (wc >> 18);
		*bytes++ = 0x80 | ((wc >> 12) & 0x3F);
		*bytes++ = 0x80 | ((wc >> 6) & 0x3F);
		*bytes++ = 0x80 | (wc & 0x3F);
	}

	*bytes = 0;
}
