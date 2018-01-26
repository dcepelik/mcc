/*
 * C Expression Parsing
 *
 * This is an expression parser based on shunting-yard algorithm [1].
 * Parsing of an expression is initiated with parse_expr, which
 * establishes an expression parsing context which is passed around
 * to functions which parse individual syntactic features of the language.
 *
 * Most functions in this module have been created to split the parsing
 * logic up, rather than to allow for code reuse.
 *
 * Operator information (``opinfos'') are declared in operator.c.
 *
 * [1] https://en.wikipedia.org/wiki/Shunting-yard_algorithm
 */

#include "array.h"
#include "debug.h"
#include "operator.h"
#include "parse-internal.h"
#include "parse.h"
#include "strbuf.h"
#include <assert.h>

/*
 * Expression parsing context. This encapsulates the state of the
 * shunting-yard algorithm as it works its way through an expression.
 * By expression I mean the whole expression which delimits a parsing
 * context, such as `a + b - 11', not  the individual partial ast_exprs
 * such as `a + b'.
 *
 * This means that when a sub-expression is parsed within an expression,
 * a new context is created, as in `1 + 17 / (2 + 13 * c) - 2'. This reflects
 * the fact that the sub-expression `2 + 13 * c' itself has to be a valid
 * expression and its parsing is independent of the parsing of the ``main''
 * expression.
 */
struct expr_ctx
{
	/*
	 * The following two stacks are used by the shunting-yard
	 * algorithm to parse expressions in infix notation.
	 */
	const struct opinfo **ops;	/* opinfo stack */
	struct ast_expr **args;		/* argument stack */

	/*
	 * The prefix flag says that if an operator follows which can
	 * either be interpreted as a prefix operator or a postfix/binary infix
	 * operator, the prior should be favoured.
	 *
	 * Also, this flag is used by `handle_lparen' to tell whether
	 * a parenthesized expression is a function call or a sub-expression.
	 */
	bool prefix;
};

/*
 * Pop an operator off the operator stack and pop the appropriate
 * number of operands off the operand stack. Construct the corresponding
 * AST node (either uop or bop node).
 */
static void pop_operator(struct parser *parser, struct expr_ctx *ctx)
{
	const struct opinfo *opinfo = array_pop(ctx->ops);
	struct ast_expr *expr;

	TMP_ASSERT(array_size(ctx->args) >= opinfo->arity);

	expr = objpool_alloc(&parser->ctx.exprs);
	switch (opinfo->arity) {
	case 1:	/* unary */
		expr->type = EXPR_TYPE_UOP;
		expr->uop.oper = opinfo->oper;
		expr->uop.expr = array_pop(ctx->args);
		break;
	case 2: /* binary */
		expr->type = EXPR_TYPE_BOP;
		expr->bop.oper = opinfo->oper;
		expr->bop.snd = array_pop(ctx->args);
		expr->bop.fst = array_pop(ctx->args);
		break;
	default:
		assert(0);
	}

	array_push(ctx->args, expr);
}

/*
 * Keep popping operators off the operator stack while the top operator's
 * priority exceeds the given lower bound. This is the core of the shunting-yard
 * algorithm.
 */
static void pop_ge_priority_operators(struct parser *p, struct expr_ctx *ctx, size_t prio)
{
	while (array_size(ctx->ops) && array_last(ctx->ops)->prio >= prio)
		pop_operator(p, ctx);
}

/*
 * Push an operator onto the operator stack according to the rules of the
 * shunting-yard algorithm.
 */
static void push_operator(struct parser *parser, struct expr_ctx *ctx, enum oper cur_op)
{
	const struct opinfo *cur_opinfo;

	cur_opinfo = &opinfo[cur_op];

	if (cur_opinfo->assoc == OPASSOC_LEFT)
		pop_ge_priority_operators(parser, ctx, cur_opinfo->prio);

	array_push(ctx->ops, cur_opinfo);
	parser_next(parser);

	/*
	 * Maintain the `prefix' flag. TODO
	 *
	 * The next operator can be a prefix operator only if it is
	 * at the very beginning, or when it follows a binary operator,
	 * or when it follows another prefix operator (all unary prefix
	 * operators are right-associative).
	 */
	if (cur_opinfo->arity == 2 || cur_opinfo->assoc == OPASSOC_RIGHT)
		ctx->prefix = true;
}

/*
 * Parse an offset operator.
 *
 * NOTE: This function is corecursive with `parse_expr'.
 */
static void parse_offset_operator(struct parser *parser, struct expr_ctx *ctx)
{
	struct ast_expr *expr;

	assert(token_is(parser->token, TOKEN_LBRACKET));
	parser_next(parser);

	/*
	 * This is correct, the [] operator is left-associative.
	 * Otherwise, we wouldn't be allowed to call `pop_ge_priority_operators'.
	 */
	pop_ge_priority_operators(parser, ctx, opinfo[OPER_OFFSET].prio);

	expr = objpool_alloc(&parser->ctx.exprs);
	expr->type = EXPR_TYPE_BOP;
	expr->bop.oper = OPER_OFFSET;
	expr->bop.fst = array_pop(ctx->args);
	expr->bop.snd = parse_expr(parser);
	array_push(ctx->args, expr);

	parser_require(parser, TOKEN_RBRACKET);
}

/*
 * Parse a function call expression.
 *
 * NOTE: This function is corecursive with `parse_expr'.
 */
static void parse_function_call_expr(struct parser *parser, struct expr_ctx *ctx)
{
	struct ast_expr *expr;
	size_t i;

	expr = objpool_alloc(&parser->ctx.exprs);
	expr->type = EXPR_TYPE_FCALL;
	expr->fcall.fptr = array_pop(ctx->args);

	expr->fcall.args = array_new(2, sizeof(*expr->fcall.args));
	for (i = 0; !token_is_eof(parser->token) && !token_is(parser->token, TOKEN_RPAREN); i++) {
		if (i > 0)
			parser_require(parser, TOKEN_COMMA);
		array_push(expr->fcall.args, parse_expr(parser));
	}
	array_push(ctx->args, expr);
}

/*
 * Parse a cast operator.
 *
 * NOTE: `parse_declspec' is reused here to parse the specifier-qualifier-list.
 *       Obviously, this will accept invalid casts such as `(volatile int j)x'.
 *       I assume it is better to accept them at the syntax level and reject
 *       them later, as it will likely yield better diagnostics.
 *
 * FIXME: I think that the following (bad) thing may happen: When the
 *        input expression is invalid in the ``right'' way, maybe the
 *        type of the cast (which is itself an expression) could end
 *        up being some other operator's operand.
 */
static void parse_cast_operator(struct parser *parser, struct expr_ctx *ctx)
{
	struct ast_expr *expr;

	expr = objpool_alloc(&parser->ctx.exprs);
	expr->type = EXPR_TYPE_CAST;
	parse_declspec(parser, &expr->dspec);
	array_push(ctx->args, expr);
	push_operator(parser, ctx, OPER_CAST);
}

/*
 * Handle the occurrence of a left opening parenthesis within an expression.
 * The `(' may be either start of a sub-expression, or a function call,
 * or a cast operator.
 */
static void handle_lparen(struct parser *parser, struct expr_ctx *ctx)
{
	assert(token_is(parser->token, TOKEN_LPAREN));
	parser_next(parser);

	if (!ctx->prefix) {
		parse_function_call_expr(parser, ctx);
		ctx->prefix = false;
	} else if (token_is_any_keyword(parser->token)) { /* TODO or a typedef'd name */
		parse_cast_operator(parser, ctx);
		/* NOTE: ctx->prefix maintained by `parse_cast_operator' */
	} else {
		array_push(ctx->args, parse_expr(parser));
		ctx->prefix = false;
	}

	parser_require(parser, TOKEN_RPAREN);
}

/*
 * Parse a primary expression.
 * See 6.5.1.
 */
static void parse_primary_expr(struct parser *parser, struct expr_ctx *ctx)
{
	struct ast_expr *expr;

	expr = objpool_alloc(&parser->ctx.exprs);

	switch (parser->token->type) {
	case TOKEN_NUMBER:
		expr->type = EXPR_TYPE_PRI_NUMBER;
		expr->number = parser->token->str;
		break;

	case TOKEN_NAME:
		expr->type = EXPR_TYPE_PRI_IDENT;
		expr->ident = symbol_get_name(parser->token->symbol);
		break;

	default:
		parse_error(parser, "primary expression was expected");
		break;
	}

	array_push(ctx->args, expr);
	ctx->prefix = false;

	parser_next(parser);
}

/*
 * This function reads tokens and constructs an expression AST.
 */
struct ast_expr *parse_expr(struct parser *parser)
{
	struct expr_ctx ctx;
	struct ast_expr *result;

	/*
	 * Initialize an expression-parsing context for this expression.
	 */
	ctx = (struct expr_ctx) {
		.ops = array_new(4, sizeof(*ctx.ops)),
		.args = array_new(4, sizeof(*ctx.args)),
		.prefix = true,
	};

	while (!token_is_eof(parser->token)) {
		/*
		 * This is the simple case. Tokens less than TOKEN_OP_CAST_MAX
		 * each translate to a single operator, regardless of
		 * context in which they occur (for example, the && operator
		 * is always logical and, no matter what). Therefore, safely
		 * cast the token to the operator and push it.
		 */
		if (parser->token->type < TOKEN_OP_CAST_MAX) {
			push_operator(parser, &ctx, (enum oper)parser->token->type); /* safe cast */
			continue;
		}

		switch (parser->token->type) {
		case TOKEN_RBRACKET:
		case TOKEN_RPAREN:
		case TOKEN_SEMICOLON:
		case TOKEN_COMMA: /* TODO */
			goto end_while;

		/* 
		 * The following tokens are ``ambiguous'' in the sense that they
		 * may either produce a prefix or a postfix/infix binary operator.
		 *
		 * Use the aforementioned `prefix' flag to decide which should be used.
		 */
		case TOKEN_PLUS:
			push_operator(parser, &ctx, ctx.prefix ? OPER_UPLUS : OPER_ADD);
			break;
		case TOKEN_MINUS:
			push_operator(parser, &ctx, ctx.prefix ? OPER_UMINUS : OPER_SUB);
			break;
		case TOKEN_PLUSPLUS:
			push_operator(parser, &ctx, ctx.prefix ? OPER_PREINC : OPER_POSTINC);
			break;
		case TOKEN_MINUSMINUS:
			push_operator(parser, &ctx, ctx.prefix ? OPER_PREDEC : OPER_POSTDEC);
			break;
		case TOKEN_AMP:
			push_operator(parser, &ctx, ctx.prefix ? OPER_ADDROF : OPER_BITAND);
			break;
		case TOKEN_ASTERISK:
			push_operator(parser, &ctx, ctx.prefix ? OPER_DEREF : OPER_MUL);
			break;
		case TOKEN_LBRACKET:
			parse_offset_operator(parser, &ctx);
			break;
		case TOKEN_LPAREN:
			handle_lparen(parser, &ctx);
			break;

		default:
			parse_primary_expr(parser, &ctx);
			break;
		}
	}

end_while:
	/*
	 * Reuse `pop_ge_priority_operators' to pop everything else off.
	 * (Every operator on the stack has priority >= 0.)
	 */
	pop_ge_priority_operators(parser, &ctx, 0);
	assert(array_size(ctx.ops) == 0);

	TMP_ASSERT(array_size(ctx.args) == 1);
	result = array_last(ctx.args);

	array_delete(ctx.ops);
	array_delete(ctx.args); /* safe to delete, result is allocated separately */

	return result;
}

void dump_expr(struct ast_expr *expr, struct strbuf *buf)
{
	size_t i;

	switch (expr->type) {
	case EXPR_TYPE_PRI_NUMBER:
		strbuf_printf(buf, "%s", expr->number);
		break;

	case EXPR_TYPE_PRI_IDENT:
		strbuf_printf(buf, "%s", expr->ident);
		break;

	case EXPR_TYPE_UOP:
		strbuf_printf(buf, "%s(", oper_to_string(expr->uop.oper));
		dump_expr(expr->uop.expr, buf);
		strbuf_printf(buf, ")");
		break;

	case EXPR_TYPE_BOP:
		strbuf_printf(buf, "%s(", oper_to_string(expr->bop.oper));
		dump_expr(expr->bop.fst, buf);
		strbuf_printf(buf, ", ");
		dump_expr(expr->bop.snd, buf);
		strbuf_printf(buf, ")");
		break;

	case EXPR_TYPE_FCALL:
		dump_expr(expr->fcall.fptr, buf);
		strbuf_printf(buf, "(");
		for (i = 0; i < array_size(expr->fcall.args); i++) {
			if (i > 0)
				strbuf_printf(buf, ", ");
			dump_expr(expr->fcall.args[i], buf);
		}
		strbuf_printf(buf, ")");
		break;

	case EXPR_TYPE_CAST:
		print_declspec(&expr->dspec, buf);
		break;

	default:
		assert(0);
	}
}
