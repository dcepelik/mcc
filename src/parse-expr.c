/*
 * parse-expr.c:
 * A rather simple, shunting-yard based parser of C expressions.
 */

#include "array.h"
#include "debug.h"
#include "operator.h"
#include "parse-internal.h"
#include "parse.h"
#include "strbuf.h"
#include <assert.h>

/*
 * Pop an operator off the operator stack and pop the appropriate
 * number of operands off the operand stack. Construct an appropriate
 * AST node (either uop or bop node).
 */
static void push_operation(struct parser *parser,
                           const struct opinfo **ops,
                           struct ast_expr **args)
{
	const struct opinfo *opinfo = array_last(ops);
	struct ast_expr *expr;

	TMP_ASSERT(array_size(args) >= opinfo->arity);

	expr = objpool_alloc(&parser->ctx.exprs);
	switch (opinfo->arity) {
	case 1:	/* unary */
		expr->type = EXPR_TYPE_UOP;
		expr->uop.oper = opinfo->oper;
		expr->uop.expr = array_last(args);
		array_pop(args);
		break;
	case 2: /* binary */
		expr->type = EXPR_TYPE_BOP;
		expr->bop.oper = opinfo->oper;
		expr->bop.snd = array_last(args);
		array_pop(args);
		expr->bop.fst = array_last(args);
		array_pop(args);
		break;
	default:
		assert(0);
	}

	array_pop(ops);
	array_push(args, expr);
}

/*
 * This function reads tokens and construct expression ASTs.
 *
 * It starts by translating tokens to corresponding operators
 * and then feeds them to the shunting-yard algorithm.
 */
struct ast_expr *parse_expr(struct parser *parser)
{
	const struct opinfo **ops;		/* operator stack */
	struct ast_expr **args;			/* operands stack */
	struct ast_expr *expr;			/* sub-expression */
	enum oper cur_op;			/* current operator */
	const struct opinfo *cur_opinfo;	/* current operator's information */
	bool prefix;				/* next operator may be a prefix operator */

	ops = array_new(4, sizeof(*ops));
	args = array_new(4, sizeof(*args));

	/*
	 * Initialize the prefix flag. The prefix flag says that if a token
	 * follows which can be understood as either a prefix operator or
	 * a suffix operator, the former should be preferred. Therefore, the
	 * flag is true initially.
	 *
	 * The flag is set at the end of the loop and before each `continue'.
	 */
	prefix = true;

	while (!token_is_eof(parser->token)) {
		/* translate token(s) to operator */
		switch (parser->token->type) {
		/*
		 * These are the simplest cases. The following tokens
		 * always translate to the same operator. Therefore,
		 * cur_op is set (by directly casting enum token to enum oper)
		 * and then processed by the shunting-yard.
		 */
		case TOKEN_OP_ADDEQ:
		case TOKEN_OP_DOT:
		case TOKEN_OP_AND:
		case TOKEN_OP_ARROW:
		case TOKEN_OP_ASSIGN:
		case TOKEN_OP_BITANDEQ:
		case TOKEN_OP_BITOR:
		case TOKEN_OP_BITOREQ:
		case TOKEN_OP_DIV:
		case TOKEN_OP_DIVEQ:
		case TOKEN_OP_EQ:
		case TOKEN_OP_GE:
		case TOKEN_OP_GT:
		case TOKEN_OP_LE:
		case TOKEN_OP_LT:
		case TOKEN_OP_MOD:
		case TOKEN_OP_MODEQ:
		case TOKEN_OP_MULEQ:
		case TOKEN_OP_NEG:
		case TOKEN_OP_NEQ:
		case TOKEN_OP_NOT:
		case TOKEN_OP_OR:
		case TOKEN_OP_SHL:
		case TOKEN_OP_SHLEQ:
		case TOKEN_OP_SHR:
		case TOKEN_OP_SHREQ:
		case TOKEN_OP_SUBEQ:
		case TOKEN_OP_XOR:
		case TOKEN_OP_XOREQ:
			cur_op = (enum oper)parser->token->type;
			break;

		/*
		 * These tokens may translate to either a prefix operator
		 * or a suffix one; use the `prefix' flag to decide as
		 * described earlier.
		 *
		 * Set cur_op and then continue to the shunting-yard.
		 */
		case TOKEN_PLUS:
			cur_op = prefix ? OPER_UPLUS : OPER_ADD;
			break;
		case TOKEN_MINUS:
			cur_op = prefix ? OPER_UMINUS : OPER_SUB;
			break;
		case TOKEN_PLUSPLUS:
			cur_op = prefix ? OPER_PREINC : OPER_POSTINC;
			break;
		case TOKEN_AMP:
			cur_op = prefix ? OPER_ADDROF : OPER_BITAND;
			break;
		case TOKEN_ASTERISK:
			cur_op = prefix ? OPER_DEREF : OPER_MUL;
			break;

		/*
		 * The `[' always designates an array offset expression.
		 * As the offset operator has the highest priority, we can
		 * bypass the shunting-yard for simplicity and construct
		 * the expression straight away.
		 *
		 * To do that, we push the operator and argument stack and
		 * rely on push_operation to construct the appropriate AST node.
		 */
		case TOKEN_LBRACKET:
			parser_next(parser);
			array_push(ops, &opinfo[OPER_OFFSET]);
			array_push(args, parse_expr(parser));
			push_operation(parser, ops, args);
			parser_require(parser, TOKEN_RBRACKET);
			continue;
		
		/*
		 * The `(' may be either start of a (sub-expression), or a function
		 * call f(...), or a (cast) TODO.
		 */
		case TOKEN_LPAREN:
			parser_next(parser);
			if (prefix)
			{
				array_push(args, parse_expr(parser));
			} else {
				DEBUG_MSG("funccall");
				expr = objpool_alloc(&parser->ctx.exprs);
				expr->type = EXPR_TYPE_FCALL;
				expr->fcall.fptr = array_last(args);
				array_pop(args);
				expr->fcall.args = array_new(2, sizeof(*expr->fcall.args));
				array_push(expr->fcall.args, parse_expr(parser));
				array_push(args, expr);
			}
			parser_require(parser, TOKEN_RPAREN);
			prefix = false;
			continue;

		case TOKEN_RBRACKET:
		case TOKEN_RPAREN:
		case TOKEN_SEMICOLON:
		case TOKEN_COMMA: /* TODO */
			goto break_while;

		default:
			if (parser->token->type == TOKEN_NUMBER) {
				expr = objpool_alloc(&parser->ctx.exprs);
				expr->type = EXPR_TYPE_PRI_NUMBER;
				expr->number = parser->token->str;
				array_push(args, expr);
			} else if (parser->token->type == TOKEN_NAME) {
				expr = objpool_alloc(&parser->ctx.exprs);
				expr->type = EXPR_TYPE_PRI_IDENT;
				expr->ident = symbol_get_name(parser->token->symbol);
				array_push(args, expr);
			}
			parser_next(parser);
			prefix = false;
			continue;
		}

		cur_opinfo = &opinfo[cur_op];

		/*
		 * (Shunting-yard)
		 */
		while (array_size(ops)
			&& array_last(ops)->prio >= cur_opinfo->prio
			&& cur_opinfo->assoc == OPASSOC_LEFT)
			push_operation(parser, ops, args);

		array_push(ops, cur_opinfo);
		parser_next(parser);

		/*
		 * The next operator can be a prefix operator only if it is
		 * the first operator (it's not, we've already pushed one),
		 * or when it follows a binary operator, or when it follows
		 * another prefix operator (all unary prefix operators are
		 * right-associative).
		 */
		if (cur_opinfo->arity == 2 || cur_opinfo->assoc == OPASSOC_RIGHT)
			prefix = true;
	}

break_while:
	while (array_size(ops) > 0)
		push_operation(parser, ops, args);

	assert(array_size(ops) == 0);
	assert(array_size(args) == 1);

	expr = array_last(args);
	array_delete(ops);
	array_delete(args);

	return expr;
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
	default:
		assert(0);
	}
}
