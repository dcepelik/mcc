#ifndef AST_H
#define AST_H

/*
 * Unify ast_nodes with expressions.
 */

#include "context.h"
#include "keyword.h"
#include "list.h"
#include "operator.h"

#include "decl.h"

enum ast_node_type
{
	AST_ARRAY,
	AST_DECL,
	AST_DECL_PART,
	AST_POINTER,
	AST_STRUCT_SPEC,
	AST_UNION_SPEC,
	AST_EXPR,
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
	struct ast_node_2 *size_expr;
	struct ast_node_2 *init;	/* initializer */
	struct ast_node *spec;	/* struct or union specifiaction */
	struct ast_node *decl;	/* declarator */
	struct ast_node **parts;
	struct ast_node **decls;

	union
	{
		struct ast_expr *expr;
	};
};

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
		struct ast_node_2 *su_spec;
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
	char *name;
	struct ast_node_2 **members;
};

/*
 * Type of a C declarator. Used to dispatch the union in `struct ast_declr'.
 */
enum declr_type
{
	DECLR_TYPE_ARRAY,
	DECLR_TYPE_IDENT,
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
		struct ast_node_2 *size;	/* DECLR_TYPE_ARRAY */
		char *ident;		/* DECLR_TYPE_IDENT */
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
	struct ast_declr **declrs;	/* declarators */
	struct ast_node_2 *init;	/* initializer */
};

/*
 * C declaration.
 * See 6.7.
 */
struct ast_decl
{
	struct ast_declspec declspec;		/* declaration specifiers */
	struct ast_init_declr *init_declrs;	/* init-declarators */
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
	struct ast_node_2 *cond;	/* the condition */
	struct ast_node_2 *yes;	/* expression when the condition holds */
	struct ast_node_2 *no;	/* expression when the condition does not hold */
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
	struct ast_node_2 *expr;	/* expression to be cast */
};

/*
 * Unary operation.
 * A unary operator and a single expression operand.
 */
struct ast_uop
{
	enum oper oper;		/* the operator, see `enum oper', unary operators */
	struct ast_node_2 *expr;	/* the (only) operand */
};

/*
 * Binary operation.
 * A binary operator and two ordered expressions as arguments.
 */
struct ast_bop
{
	enum oper oper;		/* the operator, see `enum oper', binary operators */
	struct ast_node_2 *fst;	/* the first operand */
	struct ast_node_2 *snd;	/* the second operand */
};

/*
 * Type of AST node. Used to dispatch the union in `struct ast_node_2'.
 *
 * NOTE: For brevity, names of enumeration constants defined by this enum
 *       do not start with the enum's name.
 */
enum ast_node_2_type
{
	AST_SU_SPEC,		/* structure or union specifier */

	/* primary expressions, see `struct ast_primary' */
	AST_EXPR_PRI_IDENT,
	AST_EXPR_PRI_NUMBER,	/* TODO */
	/* ... */
	AST_EXPR_PRI_GENERIC,

	AST_EXPR_COND,

	/* ... */

	AST_EXPR_UOP,		/* unary operation */
	AST_EXPR_BOP,		/* binary operation */

	AST_UNIT,		/* translation unit */
};

/*
 * AST node.
 */
struct ast_node_2
{
	enum ast_node_2_type type;
	union
	{
		struct ast_decl decl;		/* declaration */
		struct ast_su_spec su_spec;	/* structure or union specifier */

		struct ast_uop uop;		/* unary operation */
		struct ast_bop bop;		/* binary operation */
		struct ast_cexpr cexpr;		/* conditional expression */
		struct ast_cast cast;		/* cast expression */

		char *number;			/* TODO */
	};
};

/*
 * The Abstract Syntax Tree (AST).
 */
struct ast
{
	struct ast_node root;
};


void ast_node_init(struct ast_node *node);
struct ast_node *ast_node_new(struct context *ctx, enum ast_node_type type);
struct ast_node_2 *ast_node_2_new(struct context *ctx, enum ast_node_2_type type);

#endif
