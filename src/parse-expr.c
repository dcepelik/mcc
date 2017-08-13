#include "parse.h"
#include "parse-internal.h"
#include "operator.h"

#define OPSTACK_SIZE	16


struct ast_expr *parse_expr(struct parser *parser)
{
	const struct opinfo **ops = NULL;
	size_t ops_count = 0;
	size_t ops_size = 4;
	const struct opinfo *cur_op = NULL;

	/*
	 * 1. translate tokens to opinfos
	 * 2. push opinfos
	 */

	while (!token_is_eof(parser->token)) {
		if (parser->token->type < TOKEN_OP_CAST_MAX) {
			cur_op = &opinfo[(enum oper)parser->token->type];
		}
		else {
			printf("%s ", token_to_string(parser->token));
			parser_next(parser);
			continue;
		}

		while (ops_count && ops[ops_count - 1]->prio >= cur_op->prio) {
			ops_count--;
			printf("oper(%i) ", ops[ops_count]->oper);
		}

		if (!ops || ops_count == ops_size) {
			ops_size *= 2;
			ops = realloc(ops, ops_size * sizeof(*ops));
		}
		ops[ops_count++] = cur_op;

		parser_next(parser);
	}

	while (ops_count > 0) {
		ops_count--;
		printf("oper(%i) ", ops[ops_count]->oper);
	}

	return NULL;
}


void dump_expr(struct ast_expr *expr, struct strbuf *buf)
{
	(void) expr;
	(void) buf;
}
