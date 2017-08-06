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

enum kwd
{
	KWD_AUTO, KWD_BREAK, KWD_CASE, KWD_CHAR, KWD_CONST, KWD_CONTINUE, KWD_DEFAULT,
	KWD_DO, KWD_DOUBLE, KWD_ELSE, KWD_ENUM, KWD_EXTERN, KWD_FLOAT, KWD_FOR, KWD_GOTO,
	KWD_IF, KWD_INLINE, KWD_INT, KWD_LONG, KWD_REGISTER, KWD_RESTRICT, KWD_RETURN,
	KWD_SHORT, KWD_SIGNED, KWD_SIZEOF, KWD_STATIC, KWD_STRUCT, KWD_SWITCH,
	KWD_TYPEDEF, KWD_UNION, KWD_UNSIGNED, KWD_VOID, KWD_VOLATILE, KWD_WHILE,
	KWD_ALIGNAS, KWD_ALIGNOF, KWD_ATOMIC, KWD_BOOL, KWD_COMPLEX, KWD_GENERIC,
	KWD_IMAGINARY, KWD_NORETURN, KWD_STATIC_ASSERT, KWD_THREAD_LOCAL
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
 * C keyword information.
 *
 * Defined at compile time, the kwdinfo structures are used to configure the symbol
 * table and to simplify the parsing of declaration specifiers, among other things.
 */
struct kwdinfo
{
	char *name;		/* name of the keyword */
	enum kwd_class class;	/* keyword class */
	enum kwd kwd;		/* keyword type */
	union
	{
		enum tqual tqual;	/* type qualifier */
		enum tspec tspec;	/* type specifier, see enum tspec */
		enum tflag tflags;	/* type specifier, see enum tflag */ /* TODO rename to tflag */
		enum storcls storcls;	/* storage class */
	};
};

extern const struct kwdinfo kwdinfo[44];

#endif
