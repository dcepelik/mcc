#include "operator.h"
#include "token.h"
#include <assert.h>

const struct opinfo opinfo[NOPERS] = {
	[OPER_ADDEQ]	= { OPER_ADDEQ,		2,	2,	OPASSOC_RIGHT },
	[OPER_ADDROF]	= { OPER_ADDROF,	1,	13,	OPASSOC_RIGHT },
	[OPER_ADD]	= { OPER_ADD,		2,	11,	OPASSOC_LEFT },
	[OPER_ALIGNOF]	= { OPER_ALIGNOF,	1,	13,	OPASSOC_LEFT },
	[OPER_AND]	= { OPER_AND,		2,	4,	OPASSOC_LEFT },
	[OPER_ARROW]	= { OPER_ARROW,		2,	14,	OPASSOC_LEFT },
	[OPER_ASSIGN]	= { OPER_ASSIGN,	2,	2,	OPASSOC_RIGHT },
	[OPER_BITANDEQ]	= { OPER_BITANDEQ,	2,	2,	OPASSOC_RIGHT },
	[OPER_BITAND]	= { OPER_BITAND,	2,	7,	OPASSOC_LEFT },
	[OPER_BITOREQ]	= { OPER_BITOREQ,	2,	2,	OPASSOC_RIGHT },
	[OPER_BITOR]	= { OPER_BITOR,		2,	5,	OPASSOC_LEFT },
	[OPER_DEREF]	= { OPER_DEREF,		1,	13,	OPASSOC_RIGHT },
	[OPER_DIVEQ]	= { OPER_DIVEQ,		2,	2,	OPASSOC_RIGHT },
	[OPER_DIV]	= { OPER_DIV,		2,	12,	OPASSOC_LEFT },
	[OPER_DOT]	= { OPER_DOT,		2,	14,	OPASSOC_LEFT },
	[OPER_EQ] 	= { OPER_EQ,		2,	8,	OPASSOC_LEFT },
	[OPER_GE] 	= { OPER_GE,		2,	9,	OPASSOC_LEFT },
	[OPER_GT] 	= { OPER_GT,		2,	9,	OPASSOC_LEFT },
	[OPER_LE] 	= { OPER_LE,		2,	9,	OPASSOC_LEFT },
	[OPER_LT] 	= { OPER_LT,		2,	9,	OPASSOC_LEFT },
	[OPER_MODEQ]	= { OPER_MODEQ,		2,	2,	OPASSOC_RIGHT },
	[OPER_MOD]	= { OPER_MOD,		2,	12,	OPASSOC_LEFT },
	[OPER_MULEQ]	= { OPER_MULEQ,		2,	2,	OPASSOC_RIGHT },
	[OPER_MUL]	= { OPER_MUL,		2,	12,	OPASSOC_LEFT },
	[OPER_NEG]	= { OPER_NEG,		2,	13,	OPASSOC_RIGHT },
	[OPER_NEQ]	= { OPER_NEQ,		2,	8,	OPASSOC_LEFT },
	[OPER_NOT]	= { OPER_NOT,		2,	13,	OPASSOC_RIGHT },
	[OPER_OFFSET]	= { OPER_OFFSET,	2,	14,	OPASSOC_LEFT },
	[OPER_OR]	= { OPER_OR,		2,	3,	OPASSOC_LEFT },
	[OPER_POSTDEC]	= { OPER_POSTDEC,	1,	14,	OPASSOC_LEFT },
	[OPER_POSTINC]	= { OPER_POSTINC,	1,	14,	OPASSOC_LEFT },
	[OPER_PREDEC]	= { OPER_PREDEC,	1,	13,	OPASSOC_RIGHT },
	[OPER_PREINC]	= { OPER_PREINC,	1,	13,	OPASSOC_RIGHT },
	[OPER_SHLEQ]	= { OPER_SHLEQ,		2,	2,	OPASSOC_RIGHT },
	[OPER_SHL]	= { OPER_SHL,		2,	10,	OPASSOC_LEFT },
	[OPER_SHREQ]	= { OPER_SHREQ,		2,	2,	OPASSOC_RIGHT },
	[OPER_SHR]	= { OPER_SHR,		2,	10,	OPASSOC_LEFT },
	[OPER_SIZEOF]	= { OPER_SIZEOF,	1,	13,	OPASSOC_LEFT },
	[OPER_SUBEQ]	= { OPER_SUBEQ,		2,	2,	OPASSOC_RIGHT },
	[OPER_SUB]	= { OPER_SUB,		2,	11,	OPASSOC_LEFT },
	[OPER_UMINUS]	= { OPER_UMINUS,	1,	13,	OPASSOC_RIGHT },
	[OPER_UPLUS]	= { OPER_UPLUS,		1,	13,	OPASSOC_RIGHT },
	[OPER_XOREQ]	= { OPER_XOREQ,		2,	2,	OPASSOC_RIGHT },
	[OPER_XOR]	= { OPER_XOR,		2,	6,	OPASSOC_LEFT },
	[OPER_FCALL]	= { OPER_FCALL,		0,	14,	OPASSOC_LEFT },
	[OPER_CAST]	= { OPER_CAST,		2,	13,	OPASSOC_RIGHT },
};

const char *oper_to_string(enum oper oper)
{
	if (oper < (enum oper)TOKEN_OP_CAST_MAX)
		return token_get_name((enum token_type)oper);

	switch (oper) {
	case OPER_ADD:
		return "+";
	case OPER_ADDROF:
		return "<addrof>";
	case OPER_ALIGNOF:
		return "<alignof>";
	case OPER_BITAND:
		return "&";
	case OPER_DEREF:
		return "<deref>";
	case OPER_DOT:
		return ".";
	case OPER_MUL:
		return "*";
	case OPER_OFFSET:
		return "<offset>";
	case OPER_SIZEOF:
		return "<sizeof>";
	case OPER_SUB:
		return "-";
	case OPER_UPLUS:
		return "+<prefix>";
	case OPER_UMINUS:
		return "-<prefix>";
	case OPER_POSTINC:
		return "++<postfix>";
	case OPER_POSTDEC:
		return "--<postfix>";
	case OPER_PREINC:
		return "++<prefix>";
	case OPER_PREDEC:
		return "--<prefix>";
	case OPER_CAST:
		return "cast";
	default:
		assert(0);
	}
}
