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
	struct ast_node *node;
	struct token *last;
	struct ast_node **child = &parent->of;
	
	while (true) {
		if (token_is(parser->token, TOKEN_RPAREN)) {
			assert(!toklist_is_empty(stack));
			last = toklist_remove_last(stack);

			switch (last->type) {
			case TOKEN_ASTERISK:
				*child = ast_node_new(&parser->ctx);
				(*child)->type = AST_NODE_TYPE;
				(*child)->ctype = CTYPE_POINTER;
				child = &((*child)->ptr_to);
				break;

			case TOKEN_LPAREN:
				parser_skip(parser);
				break;

			default:
				assert(false);
			}
		}
		else if (token_is(parser->token, TOKEN_LBRACKET)) {
			parser_skip(parser);
			assert(token_is(parser->token, TOKEN_RBRACKET));
			parser_skip(parser);

			*child = ast_node_new(&parser->ctx);
			(*child)->type = AST_NODE_TYPE;
			(*child)->ctype = CTYPE_ARRAY;
			child = &((*child)->array_of);
		}
		else {
			*child = ast_node_new(&parser->ctx);
			(*child)->type = AST_NODE_TYPE;
			(*child)->ctype = CTYPE_INT;

			break;
		}
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

	while (!token_is_name(parser->token) && !token_is_eof(parser->token)) {
		toklist_insert_last(&stack, parser->token);
		parser_next(parser);
	}

	parser_skip(parser); /* name */

	decl = ast_node_new(&parser->ctx);
	decl->type = AST_NODE_DECL;
	parser_parse_declaration_do(parser, decl, &stack);

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

	dump_decl(decl->of);
}
