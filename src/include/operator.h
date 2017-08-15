#ifndef OPERATOR_H
#define OPERATOR_H

#include "common.h"

/*
 * C operators.
 * See: 6.5.2--6.5.16.
 */
enum oper
{
	/*
	 * The operators which translate directly from the corresponding
	 * TOKEN_OP_* tokens.
	 *
	 * NOTE: Please do not modify the order of items within
	 *       this block without modifying `enum token_type'
	 *       correspondingly. See `enum token_type'.
	 */
	OPER_ADDEQ,	/* x += y */
	OPER_AND,	/* x && y */
	OPER_ARROW,	/* x->y */
	OPER_ASSIGN,	/* x = y */
	OPER_BITANDEQ,	/* x &= y */
	OPER_BITOR,	/* x | y */
	OPER_BITOREQ,	/* x |= y */
	OPER_DEC,	/* x-- or --x */
	OPER_DIV,	/* x / y */
	OPER_DIVEQ,	/* x /= y */
	OPER_EQ, 	/* x == y */
	OPER_GE, 	/* x >= y */
	OPER_GT, 	/* x > y */
	OPER_INC,	/* x++ or ++x */
	OPER_LE, 	/* x <= y */
	OPER_LT, 	/* x < y */
	OPER_MOD,	/* x % y */
	OPER_MODEQ,	/* x %= y */
	OPER_MULEQ,	/* x *= y */
	OPER_NEG,	/* ~x */
	OPER_NEQ,	/* x != y */
	OPER_NOT,	/* !x */
	OPER_OR,	/* x || y */
	OPER_SHL,	/* x << y */
	OPER_SHLEQ,	/* x <<= y */
	OPER_SHR,	/* x >> y */
	OPER_SHREQ,	/* x >>= y */
	OPER_SUBEQ,	/* x -= y */
	OPER_XOR,	/* x ^ y */
	OPER_XOREQ,	/* x ^= y */

	/*
	 * The remaining (context-sensitive) operators, or operators
	 * which do not correspond to a single simple token.
	 *
	 * For example, OPER_ALIGNOF translates from a TOKEN_NAME
	 * token whose symbolic name is `_Alignof', and OPER_OFFSET
	 * consists of multiple tokens (`[' and `]' with an expression
	 * in between).
	 */

	OPER_ADD,	/* x + y */
	OPER_ADDROF,	/* &x */
	OPER_ALIGNOF,	/* _Alignof(x) */
	OPER_BITAND,	/* x & y */
	OPER_DEREF,	/* *x */
	OPER_DOT,	/* x.y */
	OPER_MUL,	/* x * y */
	OPER_OFFSET,	/* x[y] */
	OPER_SIZEOF,	/* sizeof(x) */
	OPER_SUB,	/* x - y */
	OPER_UMINUS,	/* -x */
	OPER_UPLUS,	/* +x */
};

/*
 * C operator information.
 */
struct opinfo
{
	byte_t oper;	/* the operator, see `enum oper' */
	byte_t arity;	/* arity */
	byte_t prio;	/* priority */
};

extern const struct opinfo opinfo[42];

const char *oper_to_string(enum oper oper);

#endif
