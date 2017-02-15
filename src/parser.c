#include "parser.h"


static void parser_next(struct parser *parser)
{
	parser->token = cpp_next(parser->cpp);
	DEBUG_MSG("Current token: ");
	token_dump(parser->token, stderr);
}


/*
 * Wrapper arround parser_next. To make it stand out when tokens
 * are deliberately skipped.
 */
static void parser_skip(struct parser *parser)
{
	parser_next(parser);
}


static void parser_next_push(struct parser *parser, struct toklist *toklist)
{
	toklist_insert_last(toklist, parser->token);
	parser_next(parser);
}


void parser_init(struct parser *parser)
{
	context_init(&parser->ctx);
	parser->cpp = cpp_new(&parser->ctx);
}


void parser_free(struct parser *parser)
{
	context_free(&parser->ctx);
	cpp_delete(parser->cpp);
}


static bool parser_is_eof(struct parser *parser)
{
	return token_is(parser->token, TOKEN_EOF);
}


static void parser_skip_rest(struct parser *parser)
{
}


static struct ast_node *parser_parse_declaration_part(struct parser *parser, struct toklist *stack)
{
	struct ast_node *node;
	struct token *top;

	node = ast_node_new(&parser->ctx);
	node->node_type = AST_NODE_TYPE;

again:
	if (token_is(parser->token, TOKEN_RPAREN)) {
		assert(!toklist_is_empty(stack));
		top = toklist_remove_last(stack);

		if (token_is_keyword(top, "const")) {
			node->const_flag = true;
			goto again;
		}
		else if (top->type == TOKEN_ASTERISK) {
			node->ctype = CTYPE_POINTER;
		}
		else if (top->type == TOKEN_LPAREN) {
			parser_next(parser);
			DEBUG_MSG("Again");
			goto again;
		}
		else {
			assert(false);
		}
	}
	else if (token_is(parser->token, TOKEN_LBRACKET)) {
		parser_next(parser);

		if (token_is(parser->token, TOKEN_NUMBER)) {
			node->size = atoi(parser->token->str);
			parser_next(parser);
		}

		assert(token_is(parser->token, TOKEN_RBRACKET));
		parser_next(parser);

		node->ctype = CTYPE_ARRAY;
	}
	else {
		node->ctype = CTYPE_INT;
	}

	return node;
}


/*
 * Actually parse the declaration.
 *
 * Examples of various declarations:
 *
 *     int a
 *     int * const *b[]
 *     const int * const c
 *     volatile int *d[][]
 *     int *(*f(int a))
 *
 */
static void parser_parse_declaration_do(struct parser *parser, struct ast_node *parent, struct toklist *stack)
{
	struct ast_node *prev = parent;
	struct ast_node *child;

	while (true) {
		if (prev->node_type == AST_NODE_TYPE && prev->ctype == CTYPE_INT)
			break;

		child = parser_parse_declaration_part(parser, stack);
		prev->type = child;
		prev = child;

		DEBUG_MSG("Through");
	}
}


/*
 * Buffer tokens up to the name (or EOF), let others do the hard work
 * and then synthesize the final result.
 */
struct ast_node *parser_parse_declaration(struct parser *parser)
{
	struct toklist stack;
	struct ast_node *decl;
	
	toklist_init(&stack);

	while ((!token_is_name(parser->token) || token_is_keyword(parser->token, "const"))  && !token_is_eof(parser->token)) {
		toklist_insert_last(&stack, parser->token);
		parser_next(parser);
	}

	parser_skip(parser); /* name */

	decl = ast_node_new(&parser->ctx);
	decl->node_type = AST_NODE_DECL;
	parser_parse_declaration_do(parser, decl, &stack);

	toklist_free(&stack);

	return decl;
}


void dump_decl(struct ast_node *decl)
{
	assert(decl->node_type == AST_NODE_TYPE);

	if (decl->const_flag)
		fprintf(stderr, "constant ");

	switch (decl->ctype) {
	case CTYPE_POINTER:
		fprintf(stderr, "pointer to ");
		break;

	case CTYPE_ARRAY:
		fprintf(stderr, "array[%lu] of ", decl->size);
		break;

	case CTYPE_INT:
		fprintf(stderr, "int");
		return;

	default:
		fprintf(stderr, "(unknown)");
		return;
	}

	dump_decl(decl->type);
	fprintf(stderr, "\n");
}


void parser_build_ast(struct parser *parser, struct ast *tree, char *cfile)
{
	struct ast_node *decl;

	cpp_open_file(parser->cpp, cfile);
	parser_next(parser);
	decl = parser_parse_declaration(parser);
	cpp_close_file(parser->cpp);

	dump_decl(decl->type);
}
