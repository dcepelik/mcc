#ifndef AST_H
#define AST_H

/*
 * Unify ast_nodes with expressions.
 */

#include "context.h"
#include "decl.h"
#include "keyword.h"
#include "list.h"
#include "operator.h"

/*
 * C declaration specifiers.
 * See 6.7.
 */
struct ast_declspec
{
	decl_tspec_t tspec;	/* type specifiers, see `enum tspec' */
	decl_tflags_t tflags;	/* type flags, see `enum tflag' */
	decl_tquals_t tquals;	/* type qualifiers, see `enum tquals' */
	decl_storcls_t storcls;	/* storage class, see `enum storcls' */
	union
	{
		struct ast_su_spec *su_spec;
	};
};

/*
 * C struct or union specifier.
 * See 6.7.2.1.
 *
 * NOTE: The owner of the structure is supposed to know whether the
 *       specifier specifies a struct or a union.
 */
struct ast_su_spec
{
	char *name;			/* name of the structure or union */
	struct ast_decl *members;	/* member declarations */
};

/*
 * Type of a C declarator. Used to dispatch the union in `struct ast_declr'.
 */
enum declr_type
{
	DECLR_TYPE_ARRAY,
	DECLR_TYPE_PTR,
};

/*
 * C declarator.
 * See 6.7.6.
 *
 * NOTE: `ast_declr' is used as an abbreviation of `declarator', while `decl'
 *       stands for `declaration'.
 */
struct ast_declr
{
	union
	{
		struct ast_expr *size;	/* DECLR_TYPE_ARRAY */
		decl_tquals_t tquals;	/* DECLR_TYPE_PTR */
	};
	byte_t type;		/* type of the declarator, see `enum declr_type' */
};

/*
 * C init-declarator (a declarator and possibly an initializer).
 * See 6.7.
 */
struct ast_init_declr
{
	char *ident;			/* identifier */
	struct ast_declr *declrs;	/* array of declarators */
	struct ast_expr *init;		/* optional initializer */
};

/*
 * C declaration.
 * See 6.7.
 */
struct ast_decl
{
	struct ast_declspec declspec;		/* declaration specifiers */
	struct ast_init_declr *init_declrs;	/* init-declarator-list */
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
 */
struct ast_fcall
{
	struct ast_expr *fptr;	/* function pointer expression */
	struct ast_expr **args;	/* function arguments */
};

/*
 * Type of a C expression. Used to dispatch the union in `struct ast_expr'.
 */
enum expr_type
{
	/* primary expressions, see `struct ast_primary' */
	EXPR_TYPE_PRI_IDENT,
	EXPR_TYPE_PRI_NUMBER,	/* TODO */
	/* ... */
	EXPR_TYPE_PRI_GENERIC,

	EXPR_TYPE_COND,

	/* ... */

	EXPR_TYPE_UOP,		/* unary operation */
	EXPR_TYPE_BOP,		/* binary operation */
	EXPR_TYPE_FCALL,	/* function call */
	EXPR_TYPE_CAST,		/* cast expression */
};

/*
 * C expression.
 * See 6.5.1--6.6.
 */
struct ast_expr
{
	enum expr_type type;
	union {
		struct ast_cexpr cexpr;		/* conditional expression */
		struct ast_declspec dspec;	/* cast expression */
		struct ast_uop uop;		/* unary operation */
		struct ast_bop bop;		/* binary operation */
		struct ast_fcall fcall;		/* function call */
		char *number;			/* TODO number string */
		char *ident;			/* TODO identifier */
	};
};

struct ast
{
	int foo;
};

#endif
