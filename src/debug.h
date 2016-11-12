#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

#ifndef DEBUG
#define DEBUG	0
#endif

#define DEBUG_PRINTF(fmt, ...) do { \
	if (DEBUG) fprintf(stderr, "*** DEBUG OUTPUT ***\t" fmt " at %s:%d\n", \
				__VA_ARGS__, __FILE__, __LINE__); \
} while (0);

#define DEBUG_MSG(msg)			DEBUG_PRINTF("%s", msg);
#define DEBUG_EXPR(formatter, expr)	DEBUG_PRINTF(#expr " = " formatter, (expr));

#endif
