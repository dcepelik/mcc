#ifndef DECL_H
#define DECL_H

#include <inttypes.h>

/*
 * TODO Find a better name for this include.
 */

/*
 * Type to hold `enum tspec'.
 */
typedef uint16_t	decl_tspec_t;

/*
 * Type to hold `enum tflag'.
 */
typedef uint8_t		decl_tflags_t;

/*
 * Type to hold `enum tquals'.
 */
typedef uint8_t		decl_tquals_t;

/*
 * Type to hold `enum storcls'.
 */
typedef uint8_t		decl_storcls_t;

/*
 * C Type Specifiers which are considered ``basic types''. They are accompanied by
 * ``flags'' which modify them, see `enum tflag'.
 *
 * NOTE: We are distinguishing ``basic types'' and ``flags'' for convenience only.
 *       Together they are what the C Standard calls ``type specifiers''.
 */
enum tspec
{
	TSPEC_BOOL	= 1 << 0, 
	TSPEC_CHAR	= 1 << 1,
	TSPEC_DOUBLE	= 1 << 2,
	TSPEC_ENUM	= 1 << 3,
	TSPEC_FLOAT	= 1 << 4,
	TSPEC_INT	= 1 << 5,
	TSPEC_STRUCT	= 1 << 6,
	TSPEC_UNION	= 1 << 7,
	TSPEC_VOID	= 1 << 8,
};

/*
 * C Type Specifiers which are considered merely ``flags'' accompanying the
 * ``basic types'' which they modify, see `enum tspec'.
 */
enum tflag
{
	TFLAG_UNSIGNED	= 1 << 0,
	TFLAG_SIGNED	= 1 << 1,
	TFLAG_SHORT	= 1 << 2,
	TFLAG_LONG	= 1 << 3,
	TFLAG_LONG_LONG	= 1 << 4,
	TFLAG_COMPLEX	= 1 << 5,
};

/*
 * C Type Qualifiers.
 */
enum tqual
{
	TQUAL_CONST	= 1 << 0,
	TQUAL_RESTRICT	= 1 << 1,
	TQUAL_VOLATILE	= 1 << 2,
	TQUAL_ATOMIC	= 1 << 3,
};

/*
 * C Storage Class Specifiers.
 */
enum storcls
{
	STORCLS_AUTO		= 1 << 0,
	STORCLS_EXTERN		= 1 << 1,
	STORCLS_INLINE		= 1 << 2,
	STORCLS_REGISTER	= 1 << 3,
	STORCLS_STATIC		= 1 << 4,
	STORCLS_THREAD_LOCAL	= 1 << 5,
};

#endif
