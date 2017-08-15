#include "operator.h"
#include "token.h"
#include <assert.h>


const struct opinfo opinfo[42] = {
	[OPER_ADDEQ]	= { OPER_ADDEQ,		2,	2 },
	[OPER_ADDROF]	= { OPER_ADDROF,	1,	13 },
	[OPER_ADD]	= { OPER_ADD,		2,	11 },
	[OPER_ALIGNOF]	= { OPER_ALIGNOF,	1,	13 },
	[OPER_AND]	= { OPER_AND,		2,	4 },
	[OPER_ARROW]	= { OPER_ARROW,		2,	14 },
	[OPER_ASSIGN]	= { OPER_ASSIGN,	2,	2 },
	[OPER_BITANDEQ]	= { OPER_BITANDEQ,	2,	2 },
	[OPER_BITAND]	= { OPER_BITAND,	2,	7 },
	[OPER_BITOREQ]	= { OPER_BITOREQ,	2,	2 },
	[OPER_BITOR]	= { OPER_BITOR,		2,	5 },
	[OPER_DEC]	= { OPER_DEC,		2,	13 },
	[OPER_DEREF]	= { OPER_DEREF,		1,	13 },
	[OPER_DIVEQ]	= { OPER_DIVEQ,		2,	2 },
	[OPER_DIV]	= { OPER_DIV,		2,	12 },
	[OPER_DOT]	= { OPER_DOT,		2,	14 },
	[OPER_EQ] 	= { OPER_EQ,		2,	8 },
	[OPER_GE] 	= { OPER_GE,		2,	9 },
	[OPER_GT] 	= { OPER_GT,		2,	9 },
	[OPER_INC]	= { OPER_INC,		1,	13 },
	[OPER_LE] 	= { OPER_LE,		2,	9 },
	[OPER_LT] 	= { OPER_LT,		2,	9 },
	[OPER_MODEQ]	= { OPER_MODEQ,		2,	2 },
	[OPER_MOD]	= { OPER_MOD,		2,	12 },
	[OPER_MULEQ]	= { OPER_MULEQ,		2,	2 },
	[OPER_MUL]	= { OPER_MUL,		2,	12 },
	[OPER_NEG]	= { OPER_NEG,		2,	13 },
	[OPER_NEQ]	= { OPER_NEQ,		2,	8 },
	[OPER_NOT]	= { OPER_NOT,		2,	13 },
	[OPER_OFFSET]	= { OPER_OFFSET,	2,	14 },
	[OPER_OR]	= { OPER_OR,		2,	3 },
	[OPER_SHLEQ]	= { OPER_SHLEQ,		2,	2 },
	[OPER_SHL]	= { OPER_SHL,		2,	10 },
	[OPER_SHREQ]	= { OPER_SHREQ,		2,	2 },
	[OPER_SHR]	= { OPER_SHR,		2,	10 },
	[OPER_SIZEOF]	= { OPER_SIZEOF,	1,	13 },
	[OPER_SUBEQ]	= { OPER_SUBEQ,		2,	2 },
	[OPER_SUB]	= { OPER_SUB,		2,	11 },
	[OPER_UMINUS]	= { OPER_UMINUS,	1,	13 },
	[OPER_UPLUS]	= { OPER_UPLUS,		1,	13 },
	[OPER_XOREQ]	= { OPER_XOREQ,		2,	2 },
	[OPER_XOR]	= { OPER_XOR,		2,	6 },
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
		return "U+";
	case OPER_UMINUS:
		return "U-";
	default:
		assert(0);
	}
}
