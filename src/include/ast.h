#ifndef AST_H
#define AST_H

#include "context.h"
#include "keyword.h"

enum ast_node_type
{
	AST_ARRAY,
	AST_DECL,
	AST_DECL_PART,
	AST_POINTER,
	AST_STRUCT_SPEC,
	AST_UNION_SPEC,
};

struct ast_node
{
	enum ast_node_type type;

	struct {
		uint16_t tspec;		/* see enum tspec */
		uint8_t tflags;		/* see enum tflags */
		uint8_t tquals;		/* see enum tquals */
		uint8_t storcls;	/* see enum storcls */
	};
	char *ident;
	size_t size;
	struct ast_node *spec;	/* struct or union specifiaction */
	struct ast_node *decl;	/* declarator */
	struct ast_node **parts;
	struct ast_node **decls;
};

enum expr_type
{
	/* primary expressions, see ``struct ast_primary'' */
	EXPR_TYPE_PRI_IDENT,
	/* ... */
	EXPR_TYPE_PRI_GENERIC,

	EXPR_TYPE_COND,

	/* ... */

	EXPR_TYPE_BEXPR,
	EXPR_TYPE_UEXPR,
};

/*
 * Primary expression.
 * See 6.5.1.
 */
struct ast_primary
{
	union {
		char *ident;	/* an identifier */
		/* ... */
	};
};

/*
 * Conditional expression.
 * The condition and two expressions for when it holds and when it does not.
 *
 * See 6.5.15.
 */
struct ast_cexpr
{
	struct ast_expr *cond;	/* the condition */
	struct ast_expr *yes;	/* expression when the condition holds */
	struct ast_expr *no;	/* expression when the condition does not hold */
};

/*
 * Cast expression.
 * A type name and the expression to be cast to that type.
 *
 * See 6.5.4.
 */
struct ast_cast
{
	char *type_name;	/* target type name */
	struct ast_expr *expr;	/* expression to be cast */
};

/*
 * C operators.
 * See 6.5.2--6.5.16.
 */
enum ast_oper
{
	/* unary operators, see 6.5.2--6.5.4. */
	AST_OPER_SIZEOF,	/* sizeof(x) */
	AST_OPER_ALIGNOF,	/* _Alignof(x) */
	AST_OPER_DEREF,		/* *x */
	AST_OPER_ADDR,		/* &x */
	AST_OPER_INC,		/* x++  ++x */
	AST_OPER_DEC,		/* x--  --x */
	AST_OPER_NEG,		/* ~x */
	AST_OPER_NOT,		/* !x */
	AST_OPER_UMINUS,	/* -x */
	AST_OPER_UPLUS,		/* +x */

	/* binary operators, see 6.5.2 and 6.5.5--6.5.16. */
	AST_OPER_FOLLOW,	/* x->y */
	AST_OPER_OFFSET,	/* x[y] */
	AST_OPER_DOT,		/* x.y */
	AST_OPER_MUL,		/* x * y */
	AST_OPER_DIV,		/* x / y */
	AST_OPER_MOD,		/* x % y */
	AST_OPER_ADD,		/* x + y */
	AST_OPER_SHL,		/* x << y */
	AST_OPER_SHR,		/* x >> y */
	AST_OPER_LT,		/* x < y */
	AST_OPER_LE,		/* x <= y */
	AST_OPER_GT,		/* x > y */
	AST_OPER_GE,		/* x >= y */
	AST_OPER_EQ,		/* x == y */
	AST_OPER_NEQ,		/* x != y */
	AST_OPER_BITAND,	/* x & y */
	AST_OPER_BITOR,		/* x | y */
	AST_OPER_XOR,		/* x ^ y */
	AST_OPER_AND,		/* x && y */
	AST_OPER_OR,		/* x || y */
	AST_OPER_ASSIGN,	/* x = y */
	AST_OPER_MULEQ,		/* x *= y */
	AST_OPER_DIVEQ,		/* x /= y */
	AST_OPER_MODEQ,		/* x %= y */
	AST_OPER_PLUSEQ,	/* x += y */
	AST_OPER_MINUSEQ,	/* x -= y */
	AST_OPER_SHLEQ,		/* x <<= y */
	AST_OPER_SHREQ,		/* x >>= y */
	AST_OPER_ANDEQ,		/* x &= y */
	AST_OPER_XOREQ,		/* x ^= y */
	AST_OPER_OREQ,		/* x |= y */
};

/*
 * Unary operation.
 * A unary operator and a single expression operand.
 */
struct ast_uop
{
	enum ast_oper oper;	/* the operator, see `enum ast_oper', unary operators */
	struct ast_expr *expr;	/* the (only) operand */
};

/*
 * Binary operation.
 * A binary operator and two expressions, left and right, as arguments.
 */
struct ast_bop
{
	enum ast_oper oper;	/* the operator, see `enum ast_oper', binary operators */
	struct ast_expr *left;	/* the left operand */
	struct ast_expr *right;	/* the right operand */
};

/*
 * C expression.
 * See 6.5.1--6.6.
 */
struct ast_expr
{
	enum expr_type type;
	union {
		struct ast_cexpr cexpr;	/* conditional expression */
		struct ast_cast cast;	/* cast expression */
		struct ast_uop uop;	/* unary operation */
		struct ast_bop bop;	/* binary operation */
	};
};

void ast_node_init(struct ast_node *node);
struct ast_node *ast_node_new(struct context *ctx, enum ast_node_type type);

struct ast
{
	struct ast_node root;
};

#endif
