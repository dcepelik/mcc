#include "keyword.h"


static const struct kwd keywords[] = {
	{
		.name = "_Alignas",
		.type = KWD_TYPE_ALIGNAS,
	},
	{
		.name = "_Alignof",
		.type = KWD_TYPE_ALIGNOF,
	},
	{
		.name = "_Atomic",
		.type = KWD_TYPE_ATOMIC,
	},
	{
		.name = "_Bool",
		.type = KWD_TYPE_BOOL,
	},
	{
		.name = "_Complex",
		.type = KWD_TYPE_COMPLEX,
	},
	{
		.name = "_Generic",
		.type = KWD_TYPE_GENERIC,
	},
	{
		.name = "_Imaginary",
		.type = KWD_TYPE_IMAGINARY,
	},
	{
		.name = "_Noreturn",
		.type = KWD_TYPE_NORETURN,
	},
	{
		.name = "_Static_assert",
		.type = KWD_TYPE_STATIC_ASSERT,
	},
	{
		.name = "_Thread_local",
		.type = KWD_TYPE_THREAD_LOCAL,
		.class = KWD_CLASS_STORAGE;
		.storcls = STORCLS_THREAD_LOCAL,
	},
	{
		.name = "auto",
		.type = KWD_TYPE_AUTO,
		.class = KWD_CLASS_STORAGE;
		.storcls = STORCLS_AUTO,
	},
	{
		.name = "break",
		.type = KWD_TYPE_BREAK,
	},
	{
		.name = "case",
		.type = KWD_TYPE_CASE,
	},
	{
		.name = "char",
		.type = KWD_TYPE_CHAR,
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
	},
	{
		.name = "default",
		.type = KWD_TYPE_DEFAULT,
	},
	{
		.name = "do",
		.type = KWD_TYPE_DO,
	},
	{
		.name = "double",
		.type = KWD_TYPE_DOUBLE,
	},
	{
		.name = "else",
		.type = KWD_TYPE_ELSE,
	},
	{
		.name = "enum",
		.type = KWD_TYPE_ENUM,
	},
	{
		.name = "extern",
		.type = KWD_TYPE_EXTERN,
		.class = KWD_CLASS_STORAGE,
		.storcls = STORCLS_EXTERN,
	},
	{
		.name = "float",
		.type = KWD_TYPE_FLOAT,
	},
	{
		.name = "for",
		.type = KWD_TYPE_FOR,
	},
	{
		.name = "goto",
		.type = KWD_TYPE_GOTO,
	},
	{
		.name = "if",
		.type = KWD_TYPE_IF,
	},
	{
		.name = "inline",
		.type = KWD_TYPE_INLINE,
		.class = KWD_CLASS_STORAGE,
		.storcls = STORCLS_INLINE,
	},
	{
		.name = "int",
		.type = KWD_TYPE_INT,
	},
	{
		.name = "long",
		.type = KWD_TYPE_LONG,
	},
	{
		.name = "register",
		.type = KWD_TYPE_REGISTER,
		.class = KWD_CLASS_STORAGE,
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
	},
	{
		.name = "short",
		.type = KWD_TYPE_SHORT,
	},
	{
		.name = "signed",
		.type = KWD_TYPE_SIGNED,
	},
	{
		.name = "sizeof",
		.type = KWD_TYPE_SIZEOF,
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
	},
	{
		.name = "switch",
		.type = KWD_TYPE_SWITCH,
	},
	{
		.name = "typedef",
		.type = KWD_TYPE_TYPEDEF,
	},
	{
		.name = "union",
		.type = KWD_TYPE_UNION,
	},
	{
		.name = "unsigned",
		.type = KWD_TYPE_UNSIGNED,
	},
	{
		.name = "void",
		.type = KWD_TYPE_VOID,
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
	},
};
