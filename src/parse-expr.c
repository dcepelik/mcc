#include "array.h"
#include "operator.h"
#include "parse-internal.h"
#include "parse.h"

#define OPSTACK_SIZE	16


struct ast_expr *parse_expr(struct parser *parser)
{
	const struct opinfo **ops = NULL;	/* operator stack */
	size_t ops_count = 0;			/* number of operators on the stack */
	size_t ops_size = 4;			/* size of the stack */
	enum oper oper;				/* current operator */
	const struct opinfo *cur_op = NULL;	/* current operator's information */
	bool unary = true;			/* next operator is unary */
	struct ast_expr **args;
	struct ast_expr *expr;

	args = array_new(4, sizeof(*args));

	while (!token_is_eof(parser->token)) {
again:
		/* translate token(s) to operator */
		if (parser->token->type < TOKEN_OP_CAST_MAX) {
			oper = (enum oper)parser->token->type;
		}
		else {
			switch (parser->token->type) {
			case TOKEN_PLUS:
				oper = unary ? OPER_UPLUS : OPER_ADD;
				break;
			case TOKEN_ASTERISK:
				oper = unary ? OPER_DEREF : OPER_MUL;
				break;
			case TOKEN_LBRACKET:
				oper = OPER_OFFSET;
				break;
			case TOKEN_LPAREN:
				parser_next(parser);
				parse_expr(parser);
				if (!parser_expect(parser, TOKEN_RPAREN))
					parse_error(parser, "expected )");
				unary = false;
				goto again;
			case TOKEN_RBRACKET:
			case TOKEN_RPAREN:
				goto pop_rest;
			default:
				expr = objpool_alloc(&parser->ctx.exprs);
				array_push(args, expr);
				printf("%s ", token_to_string(parser->token));
				unary = false;
				parser_next(parser);
				continue;
			}
		}

		cur_op = &opinfo[oper];
		unary = true;

		while (ops_count && ops[ops_count - 1]->prio >= cur_op->prio) {
			ops_count--;
			DEBUG_TRACE;
			printf("%s ", oper_to_string(ops[ops_count]->oper));
		}

		if (!ops || ops_count == ops_size) {
			ops_size *= 2;
			ops = realloc(ops, ops_size * sizeof(*ops));
		}
		ops[ops_count++] = cur_op;

		parser_next(parser);

		if (oper == OPER_OFFSET)
			parse_expr(parser);
	}

pop_rest:
	while (ops_count > 0) {
		ops_count--;
		printf("%s ", oper_to_string(ops[ops_count]->oper));
	}

	return NULL;
}


void dump_expr(struct ast_expr *expr, struct strbuf *buf)
{
	(void) expr;
	(void) buf;
}
