#include "ast.h"
#include "cexpr.h"

bool expr_is_cexpr(struct ast_expr *expr)
{
	switch (expr->type) {
	case EXPR_TYPE_FCALL:
		return false;

	case EXPR_TYPE_PRI_IDENT:
	case EXPR_TYPE_PRI_NUMBER:
		return true;

	case EXPR_TYPE_UOP:
		return expr_is_cexpr(expr->uop.expr);
	case EXPR_TYPE_BOP:
		return expr_is_cexpr(expr->bop.fst) && expr_is_cexpr(expr->bop.snd);
	default:
		assert(0);
	}
}
