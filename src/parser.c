#include "parser.h"


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


static void parser_parse_declarator(struct parser *parser, struct ast_node *parent);


//static void parser_parse_direct_declarator(struct parser *parser, struct ast_node *parent)
//{
//again:
//	if (token_is(parser->token, TOKEN_LPAREN)) {
//		parser_next(parser);
//		parser_parse_declarator(parser, parent);
//		assert(token_is(parser->token, TOKEN_RPAREN));
//	}
//	else if (token_is_name(parser->token)) {
//	}
//	else if (token_is(parser->token, TOKEN_LBRACKET)) {
//		parser_next(parser);
//		assert(token_is(parser->token, TOKEN_RBRACKET));
//	}
//	else {
//		return;
//	}
//
//	parser_next(parser);
//	goto again;
//}
//
//
///*
// * Note: there's no one-to-one correspondence between these coroutines and the productions
// * of the language. The reason for this is that, although a regular recursive descent
// * parser is easy to write, the construction of the declarator is tangled. Put in
// * a different way, the structure of the calls may be the same for different declarators:
// * declarators *a[] and *(a[]) will result in
// *
// *     declarator -> pointer direct-declarator -> * direct-declarator [] -> * identifier []
// *
// * and
// *
// *     declarator -> pointer direct-declarator -> * ( declarator ) -> * ( direct-declarator ) ->
// *                -> * ( direct-declarator [] ) -> * ( identifier [] )
// *
// * which are produced by similar calls, but the types differ fundamentally (pointer to array
// * vs. array of pointers).
// *
// * This is caused by operator priorities, which are not reflected in the productions of the
// * language.
// *
// * For this reason, if the production
// *
// *     direct-declarator -> ( declarator )
// *
// * is not used, this function reads the identifier otherwise handled by the function
// * parser-parse-direct-declarator.
// */


static struct ast_node *parser_parse_declaration_suffixes(struct parser *parser, struct ast_node *decl)
{
	struct ast_node *node;

	if (token_is(parser->token, TOKEN_LBRACKET)) {
		parser_next(parser);
		assert(token_is(parser->token, TOKEN_RBRACKET));
		parser_next(parser);

		node = ast_node_new(&parser->ctx);
		node->type = AST_NODE_TYPE;
		node->ctype = CTYPE_ARRAY;
		node->array_of = decl;

		if (!token_is_eof(parser->token) && !token_is(parser->token, TOKEN_RPAREN))
			return parser_parse_declaration_suffixes(parser, node);

		return node;
	}
	else {
		return decl;
	}
}


/*
 * int a
 * int * const_flag *b[]
 * const_flag int * const_flag c
 * const_flag int *d[][]
 */
static struct ast_node *parser_parse_declaration_r(struct parser *parser, struct toklist *stack)
{
	struct ast_node *node;
	
	if (token_is(parser->token, TOKEN_RPAREN)) {
		switch (toklist_last(stack)->type) {
		case TOKEN_ASTERISK:
			toklist_remove_last(stack);
			node = ast_node_new(&parser->ctx);
			node->type = AST_NODE_TYPE;
			node->ctype = CTYPE_POINTER;
			node->ptr_to = parser_parse_declaration_r(parser, stack);
			return node;

		case TOKEN_LPAREN:
			toklist_remove_last(stack);
			parser_next(parser);
			return parser_parse_declaration_r(parser, stack);

		default:
			assert(false);
		}
	}
	else if (token_is(parser->token, TOKEN_LBRACKET)) {
		parser_next(parser);
		assert(token_is(parser->token, TOKEN_RBRACKET));
		parser_next(parser);

		node = ast_node_new(&parser->ctx);
		node->type = AST_NODE_TYPE;
		node->ctype = CTYPE_ARRAY;
		node->array_of = parser_parse_declaration_r(parser, stack);
	}
	else {
		node = ast_node_new(&parser->ctx);
		node->type = AST_NODE_TYPE;
		node->ctype = CTYPE_INT;
	}

	return node;
}


struct ast_node *parser_parse_declaration(struct parser *parser)
{
	struct toklist stack;
	struct ast_node *decl;
	
	toklist_init(&stack);

	while (!token_is_name(parser->token) && !token_is_eof(parser->token)) {
		toklist_insert_last(&stack, parser->token);
		parser_next(parser);
	}

	parser_next(parser);

	decl = parser_parse_declaration_r(parser, &stack);
	toklist_free(&stack);

	return decl;
}


void dump_decl(struct ast_node *decl)
{
	assert(decl->type == AST_NODE_TYPE);

	switch (decl->ctype) {
	case CTYPE_INT:
		fprintf(stderr, "int");
		break;

	case CTYPE_POINTER:
		fprintf(stderr, "pointer to ");
		dump_decl(decl->ptr_to);
		break;

	case CTYPE_ARRAY:
		fprintf(stderr, "array of ");
		dump_decl(decl->array_of);
		break;

	default:
		fprintf(stderr, "(unknown)");
		break;
	}

	fprintf(stderr, "\n");
}


void parser_build_ast(struct parser *parser, struct ast *tree, char *cfile)
{
	struct ast_node *decl;

	cpp_open_file(parser->cpp, cfile);
	parser_next(parser);
	decl = parser_parse_declaration(parser);
	cpp_close_file(parser->cpp);

	dump_decl(decl);
}
