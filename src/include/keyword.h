#ifndef KEYWORD_H
#define KEYWORD_H

#define INT_TFLAGS	(TFLAG_UNSIGNED | TFLAG_SIGNED | TFLAG_SHORT | TFLAG_LONG | TFLAG_LONG_LONG)
#define CHAR_TFLAGS	(TFLAG_UNSIGNED | TFLAG_SIGNED)
#define DOUBLE_TFLAGS	(TFLAG_LONG | TFLAG_COMPLEX)

#include "decl.h"

/*
 * C keyword.
 */
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
 * Keyword ``class''. A rough internal classification of keywords used for example
 * to simplify the parsing of declaration specifiers.
 */
enum kwd_class
{
	KWD_CLASS_ALIGNMENT,	/* the _Alignas alignment specifier (for convenience) */
	KWD_CLASS_FLOWCTL,	/* flow control keywords */
	KWD_CLASS_FUNCSPEC,	/* function specifiers */
	KWD_CLASS_OPER,		/* operators */
	KWD_CLASS_STORCLS,	/* storage classes */
	KWD_CLASS_TQUAL,	/* type qualifiers */
	KWD_CLASS_TSPEC,	/* ``basic type'' type specifiers, see enum tspec */
	KWD_CLASS_TFLAG,	/* ``flag'' type specifiers, see enum tflag */
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
