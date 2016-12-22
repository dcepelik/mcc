/*
 * String literals.
 */

#ifndef STRINGLE_H
#define STRINGLE_H

struct stringle
{
	char *bytes;	/* bytes making up the stringle */
	size_t len;	/* length, in bytes, of the stringle */
};

#endif
