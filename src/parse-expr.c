#include "array.h"
#include "debug.h"
#include "operator.h"
#include "parse-internal.h"
#include "parse.h"
#include "strbuf.h"

#define OPSTACK_SIZE	16


static struct ast_expr *build_expr_node(struct parser *parser,
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
	return expr;
}


struct ast_expr *parse_expr(struct parser *parser)
{
	struct strbuf buf;
	strbuf_init(&buf, 1024);

	const struct opinfo **ops;		/* operator stack */
	struct ast_expr **args;			/* operands stack */
	struct ast_expr *expr;			/* current expression */
	enum oper cur_op;			/* current operator */
	const struct opinfo *cur_opinfo;	/* current operator's information */
	bool unary = true;			/* next operator is unary */

	ops = array_new(4, sizeof(*ops));
	args = array_new(4, sizeof(*args));

	while (!token_is_eof(parser->token)) {
		/* translate token(s) to operator */
		switch (parser->token->type) {
		case TOKEN_OP_ADDEQ:
		case TOKEN_DOT:
		case TOKEN_OP_AND:
		case TOKEN_OP_ARROW:
		case TOKEN_OP_ASSIGN:
		case TOKEN_OP_BITANDEQ:
		case TOKEN_OP_BITOR:
		case TOKEN_OP_BITOREQ:
		case TOKEN_OP_DEC:
		case TOKEN_OP_DIV:
		case TOKEN_OP_DIVEQ:
		case TOKEN_OP_EQ:
		case TOKEN_OP_GE:
		case TOKEN_OP_GT:
		case TOKEN_OP_INC:
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
			cur_op = unary ? OPER_UPLUS : OPER_ADD;
			break;
		case TOKEN_MINUS:
			cur_op = unary ? OPER_UMINUS : OPER_SUB;
			break;
		case TOKEN_AMP:
			cur_op = unary ? OPER_ADDROF : OPER_BITAND;
			break;
		case TOKEN_ASTERISK:
			cur_op = unary ? OPER_DEREF : OPER_MUL;
			break;
		case TOKEN_LBRACKET:
			cur_op = OPER_OFFSET;
			break;
		case TOKEN_LPAREN:
			parser_next(parser);
			array_push(args, parse_expr(parser));
			if (!parser_expect(parser, TOKEN_RPAREN))
				parse_error(parser, "expected )");
			unary = false;
			continue;
		case TOKEN_RBRACKET:
		case TOKEN_RPAREN:
			goto break_while;
		default:
			if (parser->token->type == TOKEN_NUMBER) {
				expr = objpool_alloc(&parser->ctx.exprs);
				expr->type = EXPR_TYPE_PRI_NUMBER;
				expr->number = parser->token->str;
				array_push(args, expr);
			}
			unary = false;
			parser_next(parser);
			continue;
		}

		cur_opinfo = &opinfo[cur_op];
		unary = true;

		while (array_size(ops)
			&& array_last(ops)->prio >= cur_opinfo->prio
			&& array_last(ops)->assoc == OPASSOC_LEFT) {
			expr = build_expr_node(parser, ops, args);
			array_push(args, expr);
		}

		array_push(ops, cur_opinfo);
		parser_next(parser);

		if (cur_op == OPER_OFFSET) {
			TMP_ASSERT(0);
			parse_expr(parser);
		}
	}

break_while:
	while (array_size(ops) > 0) {
		expr = build_expr_node(parser, ops, args);
		array_push(args, expr);
	}

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
