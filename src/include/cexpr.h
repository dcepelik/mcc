#ifndef CEXPR_H
#define CEXPR_H

#include <stdbool.h>

/*
 * Attempt to classify the given expression @expr as one of the cexpr_cls
 * classes of constant expressions.
 */
bool expr_is_cexpr(struct ast_expr *expr);

#endif
