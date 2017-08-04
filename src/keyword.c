#include "keyword.h"


/*
 * Definition of C keywords.
 */
const struct kwd keywords[44] = {
	{
		.name = "_Alignas",
		.type = KWD_TYPE_ALIGNAS,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Alignof",
		.type = KWD_TYPE_ALIGNOF,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Atomic",
		.type = KWD_TYPE_ATOMIC,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Bool",
		.type = KWD_TYPE_BOOL,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_BOOL,
	},
	{
		.name = "_Complex",
		.type = KWD_TYPE_COMPLEX,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_COMPLEX,
	},
	{
		.name = "_Generic",
		.type = KWD_TYPE_GENERIC,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Imaginary",
		.type = KWD_TYPE_IMAGINARY,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Noreturn",
		.type = KWD_TYPE_NORETURN,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Static_assert",
		.type = KWD_TYPE_STATIC_ASSERT,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Thread_local",
		.type = KWD_TYPE_THREAD_LOCAL,
		.class = KWD_CLASS_STORCLS,
		.storcls = STORCLS_THREAD_LOCAL,
	},
	{
		.name = "auto",
		.type = KWD_TYPE_AUTO,
		.class = KWD_CLASS_STORCLS,
		.storcls = STORCLS_AUTO,
	},
	{
		.name = "break",
		.type = KWD_TYPE_BREAK,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "case",
		.type = KWD_TYPE_CASE,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "char",
		.type = KWD_TYPE_CHAR,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_CHAR,
	},
	{
		.name = "const",
		.type = KWD_TYPE_CONST,
		.class = KWD_CLASS_TQUAL,
		.tqual = TQUAL_CONST,
	},
	{
		.name = "continue",
		.type = KWD_TYPE_CONTINUE,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "default",
		.type = KWD_TYPE_DEFAULT,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "do",
		.type = KWD_TYPE_DO,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "double",
		.type = KWD_TYPE_DOUBLE,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_DOUBLE,
	},
	{
		.name = "else",
		.type = KWD_TYPE_ELSE,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "enum",
		.type = KWD_TYPE_ENUM,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "extern",
		.type = KWD_TYPE_EXTERN,
		.class = KWD_CLASS_STORCLS,
		.storcls = STORCLS_EXTERN,
	},
	{
		.name = "float",
		.type = KWD_TYPE_FLOAT,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_FLOAT,
	},
	{
		.name = "for",
		.type = KWD_TYPE_FOR,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "goto",
		.type = KWD_TYPE_GOTO,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "if",
		.type = KWD_TYPE_IF,
		.class = KWD_CLASS_OTHER,
	},
	{
		.name = "inline",
		.type = KWD_TYPE_INLINE,
		.class = KWD_CLASS_STORCLS,
		.storcls = STORCLS_INLINE,
	},
	{
		.name = "int",
		.type = KWD_TYPE_INT,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_INT,
	},
	{
		.name = "long",
		.type = KWD_TYPE_LONG,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_LONG,
	},
	{
		.name = "register",
		.type = KWD_TYPE_REGISTER,
		.class = KWD_CLASS_STORCLS,
		.storcls = STORCLS_REGISTER,
	},
	{
		.name = "restrict",
		.type = KWD_TYPE_RESTRICT,
		.class = KWD_CLASS_TQUAL,
		.storcls = TQUAL_RESTRICT,
	},
	{
		.name = "return",
		.type = KWD_TYPE_RETURN,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "short",
		.type = KWD_TYPE_SHORT,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_SHORT,
	},
	{
		.name = "signed",
		.type = KWD_TYPE_SIGNED,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_SIGNED,
	},
	{
		.name = "sizeof",
		.type = KWD_TYPE_SIZEOF,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "static",
		.type = KWD_TYPE_STATIC,
		.class = KWD_CLASS_STORCLS,
		.storcls = STORCLS_STATIC,
	},
	{
		.name = "struct",
		.type = KWD_TYPE_STRUCT,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_STRUCT,
	},
	{
		.name = "switch",
		.type = KWD_TYPE_SWITCH,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "typedef",
		.type = KWD_TYPE_TYPEDEF,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "union",
		.type = KWD_TYPE_UNION,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_UNION,
	},
	{
		.name = "unsigned",
		.type = KWD_TYPE_UNSIGNED,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_UNSIGNED,
	},
	{
		.name = "void",
		.type = KWD_TYPE_VOID,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_VOID,
	},
	{
		.name = "volatile",
		.type = KWD_TYPE_VOLATILE,
		.class = KWD_CLASS_TQUAL,
		.tqual = TQUAL_VOLATILE,
	},
	{
		.name = "while",
		.type = KWD_TYPE_WHILE,
		.class = KWD_CLASS_OTHER
	},
};

