#ifndef KEYWORD_H
#define KEYWORD_H

#define INT_TFLAGS	(TFLAG_UNSIGNED | TFLAG_SIGNED | TFLAG_SHORT | TFLAG_LONG | TFLAG_LONG_LONG)
#define CHAR_TFLAGS	(TFLAG_UNSIGNED | TFLAG_SIGNED)

/*
 * C Type Specifiers which are considered ``basic types''. They are accompanied by
 * ``flags'' which modify them, see enum tflag.
 *
 * This distinction is for the programmer's convenience only.
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
 * C Type Specifiers which are considered merely ``flags'' which accompany the
 * ``basic types'' which they modify, see enum tspec.
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

enum kwd_type
{
	KWD_TYPE_AUTO, KWD_TYPE_BREAK, KWD_TYPE_CASE, KWD_TYPE_CHAR, KWD_TYPE_CONST,
	KWD_TYPE_CONTINUE, KWD_TYPE_DEFAULT, KWD_TYPE_DO, KWD_TYPE_DOUBLE, KWD_TYPE_ELSE,
	KWD_TYPE_ENUM, KWD_TYPE_EXTERN, KWD_TYPE_FLOAT, KWD_TYPE_FOR, KWD_TYPE_GOTO,
	KWD_TYPE_IF, KWD_TYPE_INLINE, KWD_TYPE_INT, KWD_TYPE_LONG, KWD_TYPE_REGISTER,
	KWD_TYPE_RESTRICT, KWD_TYPE_RETURN, KWD_TYPE_SHORT, KWD_TYPE_SIGNED,
	KWD_TYPE_SIZEOF, KWD_TYPE_STATIC, KWD_TYPE_STRUCT, KWD_TYPE_SWITCH,
	KWD_TYPE_TYPEDEF, KWD_TYPE_UNION, KWD_TYPE_UNSIGNED, KWD_TYPE_VOID,
	KWD_TYPE_VOLATILE, KWD_TYPE_WHILE, KWD_TYPE_ALIGNAS, KWD_TYPE_ALIGNOF,
	KWD_TYPE_ATOMIC, KWD_TYPE_BOOL, KWD_TYPE_COMPLEX, KWD_TYPE_GENERIC,
	KWD_TYPE_IMAGINARY, KWD_TYPE_NORETURN, KWD_TYPE_STATIC_ASSERT,
	KWD_TYPE_THREAD_LOCAL,
};

/*
 * Keyword class.
 */
enum kwd_class
{
	KWD_CLASS_ALIGNMENT,	/* alignment specifier */
	KWD_CLASS_FLOWCTL,	/* flow control keyword */
	KWD_CLASS_FUNCSPEC,	/* function specifier */
	KWD_CLASS_OPER,		/* operator */
	KWD_CLASS_STORCLS,	/* storage class */
	KWD_CLASS_TQUAL,	/* type qualifier */
	KWD_CLASS_TSPEC,	/* type specifier, see enum tspec */
	KWD_CLASS_TFLAG,	/* type specifier, see enum tflag */
	KWD_CLASS_OTHER,
};

/*
 * C keyword.
 */
struct kwd
{
	char *name;		/* name of the keyword */
	enum kwd_class class;	/* keyword class */
	enum kwd_type type;	/* keyword type */
	union
	{
		enum tqual tqual;	/* type qualifier */
		enum tspec tspec;	/* type specifier, see enum tspec */
		enum tflag tflags;	/* type specifier, see enum tflag */ /* TODO rename to tflag */
		enum storcls storcls;	/* storage class */
	};
};

extern const struct kwd keywords[44];

#endif
