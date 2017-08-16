#ifndef AST_H
#define AST_H

#include "context.h"
#include "keyword.h"
#include "operator.h"

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
	EXPR_TYPE_PRI_NUMBER,	/* TODO */
	/* ... */
	EXPR_TYPE_PRI_GENERIC,

	EXPR_TYPE_COND,

	/* ... */

	EXPR_TYPE_UOP,		/* unary operation */
	EXPR_TYPE_BOP,		/* binary operation */
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
 * Unary operation.
 * A unary operator and a single expression operand.
 */
struct ast_uop
{
	enum oper oper;		/* the operator, see `enum oper', unary operators */
	struct ast_expr *expr;	/* the (only) operand */
};

/*
 * Binary operation.
 * A binary operator and two ordered expressions as arguments.
 */
struct ast_bop
{
	enum oper oper;		/* the operator, see `enum oper', binary operators */
	struct ast_expr *fst;	/* the first operand */
	struct ast_expr *snd;	/* the second operand */
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
		char *number;		/* TODO */
	};
};

void ast_node_init(struct ast_node *node);
struct ast_node *ast_node_new(struct context *ctx, enum ast_node_type type);
struct ast_expr *ast_expr_new(struct context *ctx, enum expr_type type);

struct ast
{
	struct ast_node root;
};

#endif
