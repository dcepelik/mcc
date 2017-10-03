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


static void push_operation(struct parser *parser,
                           const struct opinfo **ops,
                           struct ast_expr **args)
{
	const struct opinfo *opinfo = array_last(ops);
	struct ast_expr *expr;

	assert(array_size(args) >= opinfo->arity);

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


struct ast_expr *parse_expr(struct parser *parser)
{
	const struct opinfo **ops;		/* operator stack */
	struct ast_expr **args;			/* operands stack */
	struct ast_expr *expr;			/* current expression */
	struct ast_expr *offset_expr;		/* offset expression */
	enum oper cur_op;			/* current operator */
	const struct opinfo *cur_opinfo;	/* current operator's information */
	bool prefix = true;			/* next operator is a prefix operator */

	ops = array_new(4, sizeof(*ops));
	args = array_new(4, sizeof(*args));

	while (!token_is_eof(parser->token)) {
		/* translate token(s) to operator */
		switch (parser->token->type) {
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
		case TOKEN_LBRACKET:
			parser_next(parser);
			offset_expr = parse_expr(parser);
			if (!token_is(parser->token, TOKEN_RBRACKET))
				parse_error(parser, "] was expected");
			prefix = false;
			cur_op = OPER_OFFSET;
			break;
		case TOKEN_LPAREN:
			parser_next(parser);
			if (!array_size(args) || (array_size(ops) && (array_last(ops)->arity == 2
				|| array_last(ops)->assoc == OPASSOC_RIGHT)))
			{
				expr = parse_expr(parser);
				array_push(args, expr);
				prefix = false;
			} else {
				DEBUG_MSG("funccall");
				/* parse funccall */
				array_pop(args);
			}
			parser_require(parser, TOKEN_RPAREN);
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
			}
			prefix = false;
			parser_next(parser);
			continue;
		}

		cur_opinfo = &opinfo[cur_op];
		if (cur_opinfo->assoc == OPASSOC_RIGHT)
			prefix = true;

		while (array_size(ops)
			&& array_last(ops)->prio >= cur_opinfo->prio
			&& cur_opinfo->assoc == OPASSOC_LEFT)
			push_operation(parser, ops, args);

		if (cur_op == OPER_OFFSET)
			array_push(args, offset_expr);

		array_push(ops, cur_opinfo);
		parser_next(parser);
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
	switch (expr->type) {
	case EXPR_TYPE_PRI_NUMBER:
		strbuf_printf(buf, "%s", expr->number);
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
	default:
		assert(0);
	}
}
