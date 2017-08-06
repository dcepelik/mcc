#include "keyword.h"


/*
 * Definition of C keywords.
 */
const struct kwdinfo kwdinfo[44] = {
	{
		.name = "_Alignas",
		.kwd = KWD_ALIGNAS,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Alignof",
		.kwd = KWD_ALIGNOF,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Atomic",
		.kwd = KWD_ATOMIC,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Bool",
		.kwd = KWD_BOOL,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_BOOL,
	},
	{
		.name = "_Complex",
		.kwd = KWD_COMPLEX,
		.class = KWD_CLASS_TFLAG,
		.tflags = TFLAG_COMPLEX,
	},
	{
		.name = "_Generic",
		.kwd = KWD_GENERIC,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Imaginary",
		.kwd = KWD_IMAGINARY,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Noreturn",
		.kwd = KWD_NORETURN,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Static_assert",
		.kwd = KWD_STATIC_ASSERT,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "_Thread_local",
		.kwd = KWD_THREAD_LOCAL,
		.class = KWD_CLASS_STORCLS,
		.storcls = STORCLS_THREAD_LOCAL,
	},
	{
		.name = "auto",
		.kwd = KWD_AUTO,
		.class = KWD_CLASS_STORCLS,
		.storcls = STORCLS_AUTO,
	},
	{
		.name = "break",
		.kwd = KWD_BREAK,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "case",
		.kwd = KWD_CASE,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "char",
		.kwd = KWD_CHAR,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_CHAR,
	},
	{
		.name = "const",
		.kwd = KWD_CONST,
		.class = KWD_CLASS_TQUAL,
		.tqual = TQUAL_CONST,
	},
	{
		.name = "continue",
		.kwd = KWD_CONTINUE,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "default",
		.kwd = KWD_DEFAULT,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "do",
		.kwd = KWD_DO,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "double",
		.kwd = KWD_DOUBLE,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_DOUBLE,
	},
	{
		.name = "else",
		.kwd = KWD_ELSE,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "enum",
		.kwd = KWD_ENUM,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "extern",
		.kwd = KWD_EXTERN,
		.class = KWD_CLASS_STORCLS,
		.storcls = STORCLS_EXTERN,
	},
	{
		.name = "float",
		.kwd = KWD_FLOAT,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_FLOAT,
	},
	{
		.name = "for",
		.kwd = KWD_FOR,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "goto",
		.kwd = KWD_GOTO,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "if",
		.kwd = KWD_IF,
		.class = KWD_CLASS_OTHER,
	},
	{
		.name = "inline",
		.kwd = KWD_INLINE,
		.class = KWD_CLASS_STORCLS,
		.storcls = STORCLS_INLINE,
	},
	{
		.name = "int",
		.kwd = KWD_INT,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_INT,
	},
	{
		.name = "long",
		.kwd = KWD_LONG,
		.class = KWD_CLASS_TFLAG,
		.tflags = TFLAG_LONG,
	},
	{
		.name = "register",
		.kwd = KWD_REGISTER,
		.class = KWD_CLASS_STORCLS,
		.storcls = STORCLS_REGISTER,
	},
	{
		.name = "restrict",
		.kwd = KWD_RESTRICT,
		.class = KWD_CLASS_TQUAL,
		.storcls = TQUAL_RESTRICT,
	},
	{
		.name = "return",
		.kwd = KWD_RETURN,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "short",
		.kwd = KWD_SHORT,
		.class = KWD_CLASS_TFLAG,
		.tflags = TFLAG_SHORT,
	},
	{
		.name = "signed",
		.kwd = KWD_SIGNED,
		.class = KWD_CLASS_TFLAG,
		.tflags = TFLAG_SIGNED,
	},
	{
		.name = "sizeof",
		.kwd = KWD_SIZEOF,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "static",
		.kwd = KWD_STATIC,
		.class = KWD_CLASS_STORCLS,
		.storcls = STORCLS_STATIC,
	},
	{
		.name = "struct",
		.kwd = KWD_STRUCT,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_STRUCT,
	},
	{
		.name = "switch",
		.kwd = KWD_SWITCH,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "typedef",
		.kwd = KWD_TYPEDEF,
		.class = KWD_CLASS_OTHER
	},
	{
		.name = "union",
		.kwd = KWD_UNION,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_UNION,
	},
	{
		.name = "unsigned",
		.kwd = KWD_UNSIGNED,
		.class = KWD_CLASS_TFLAG,
		.tflags = TFLAG_UNSIGNED,
	},
	{
		.name = "void",
		.kwd = KWD_VOID,
		.class = KWD_CLASS_TSPEC,
		.tspec = TSPEC_VOID,
	},
	{
		.name = "volatile",
		.kwd = KWD_VOLATILE,
		.class = KWD_CLASS_TQUAL,
		.tqual = TQUAL_VOLATILE,
	},
	{
		.name = "while",
		.kwd = KWD_WHILE,
		.class = KWD_CLASS_OTHER
	},
};

