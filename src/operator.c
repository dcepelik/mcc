#include "operator.h"


const struct opinfo opinfo[42] = {
	[OPER_SIZEOF]	= { OPER_SIZEOF,	1,	10 },
	[OPER_ALIGNOF]	= { OPER_ALIGNOF,	1,	10 },
	[OPER_ADDROF]	= { OPER_ADDROF,	1,	10 },
	[OPER_DEREF]	= { OPER_DEREF,	1,	10 },
	[OPER_UPLUS]	= { OPER_UPLUS,	1,	10 },
	[OPER_UMINUS]	= { OPER_UMINUS,	1,	10 },
	/* TODO ... */
	[OPER_ADD]		= { OPER_ADD,	2,	10 },
	[OPER_SUB]		= { OPER_SUB,	2,	10 },
	[OPER_MUL]		= { OPER_MUL,	2,	11 },
	[OPER_DIV]		= { OPER_DIV,	2,	11 },
};
