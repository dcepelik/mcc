#include "parser.h"


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


static void parser_parse_declarator(struct parser *parser, struct ast_node *parent)
{
	/* TODO support other declarators */
}


static void parser_skip_rest(struct parser *parser)
{
}


static void parser_parse_func_def(struct parser *parser, struct ast_node *parent)
{
}


static void parser_parse_ext_decl(struct parser *parser, struct ast_node *parent)
{
}


static bool parser_eof(struct parser *parser)
{
	return token_is(parser->token, TOKEN_EOF);
}


static void parser_next(struct parser *parser)
{
	parser->token = cpp_next(parser->cpp);
	DEBUG_MSG("Current token: ");
	token_dump(parser->token, stderr);
}


static void parser_next_push(struct parser *parser, struct toklist *toklist)
{
	toklist_insert_last(toklist, parser->token);
	parser_next(parser);
}


static void parser_parse_declaration(struct parser *parser, struct ast_node *parent)
{
	struct toklist lhs_stack;
	struct ast_node *new_node;
	struct ast_node *type_cursor = NULL;

	toklist_init(&lhs_stack);

	while (!parser_eof(parser)) {
		if (token_is_keyword(parser->token, "int")) {
			parent->type = AST_NODE_DECLSPEC;

			new_node = ast_node_new(&parser->ctx);
			new_node->type = AST_NODE_INT;
			parent->target = new_node;
			type_cursor = new_node;
		}
		else {
			break;
		}

		parser_next(parser);
	}

	assert(type_cursor != NULL);

	while (!parser_eof(parser)) {
		/* pointer-to, left-paren and mild type-qualifier mix */
		if (token_is(parser->token, TOKEN_LPAREN)
			|| token_is(parser->token, TOKEN_ASTERISK)
			|| token_is_keyword(parser->token, "const"))
			parser_next_push(parser, &lhs_stack);
		else
			break;
	}

	assert(token_is_name(parser->token));
	parser_next(parser);

	while (!parser_eof(parser)) {
		/* whenever you see an rparen, pop lhs_stack and wrap accordingly (?) */
		if (token_is(parser->token, TOKEN_RPAREN)) {
			while (!token_is(toklist_last(&lhs_stack), TOKEN_LPAREN)) {
				assert(token_is(toklist_last(&lhs_stack), TOKEN_ASTERISK));

				new_node = ast_node_new(&parser->ctx);
				new_node->type = AST_NODE_PTR_TO;
				new_node->target = type_cursor;
				type_cursor = new_node;

				toklist_remove_last(&lhs_stack);
			}

			assert(token_is(toklist_last(&lhs_stack), TOKEN_LPAREN));
			toklist_remove_last(&lhs_stack);
		}
		else if (token_is(parser->token, TOKEN_SEMICOLON)) {
			while (!toklist_is_empty(&lhs_stack)) {
				assert(token_is(toklist_last(&lhs_stack), TOKEN_ASTERISK));

				new_node = ast_node_new(&parser->ctx);
				new_node->type = AST_NODE_PTR_TO;
				new_node->target = type_cursor;
				type_cursor = new_node;

				toklist_remove_last(&lhs_stack);
			}

			break;
		}
		else {
			break;
		}

		parser_next(parser);
	}

	assert(token_is(parser->token, TOKEN_SEMICOLON));
	assert(toklist_is_empty(&lhs_stack));
	parser_next(parser);

	parent->target = type_cursor;
}


static void parser_parse_translation_unit(struct parser *parser, struct ast_node *root)
{
	root->type = AST_NODE_TRANSLATION_UNIT;

	while (!parser_eof(parser))
		parser_parse_declaration(parser, root);
}


void parser_build_ast(struct parser *parser, struct ast *tree, char *cfile)
{
	struct ast_node *node;

	cpp_open_file(parser->cpp, cfile);

	ast_node_init(&tree->root);
	parser_next(parser);
	parser_parse_translation_unit(parser, &tree->root);

	for (node = &tree->root; node != NULL; node = node->target) {
		switch (node->type) {
		case AST_NODE_PTR_TO:
			fprintf(stderr, "pointer to ");
			break;
		case AST_NODE_DECLSPEC:
			fprintf(stderr, "declspec: ");
			break;
		case AST_NODE_INT:
			fprintf(stderr, "int ");
			break;
		default:
			fprintf(stderr, "unkwn");
		}
	}

	fprintf(stderr, "\n");

	cpp_close_file(parser->cpp);
}
