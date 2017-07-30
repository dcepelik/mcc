#include "parser.h"
#include "keyword.h"


static void parser_next(struct parser *parser)
{
	parser->token = cpp_next(parser->cpp);
}


/*
 * Wrapper arround parser_next. To make it stand out when tokens
 * are deliberately skipped.
 */
static void parser_skip(struct parser *parser)
{
	(void) parser_next(parser);
}


static void parser_next_push(struct parser *parser, struct toklist *toklist)
{
	toklist_insert_last(toklist, parser->token);
	parser_next(parser);
}


void parser_setup_symtab(struct symtab *table)
{
	struct symbol *symbol;
	struct symdef *def;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(keywords); i++) {
		symbol = symtab_insert(table, keywords[i].name);

		def = symbol_define(table, symbol);
		def->type = SYMBOL_TYPE_C_KEYWORD;
		def->keyword = &keywords[i];
	}
}


void parser_init(struct parser *parser)
{
	context_init(&parser->ctx);
	parser_setup_symtab(&parser->ctx.symtab);
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


static void parser_parse_type_qual(struct parser *parser, struct ast_node *type, struct toklist *stack)
{
	struct token *top;

	while (!toklist_is_empty(stack)) {
		top = toklist_last(stack);

		if (token_is_keyword(top, KWD_TYPE_CONST))
			type->flags |= TYPE_FLAG_CONST;
		else if (token_is_keyword(top, KWD_TYPE_VOLATILE))
			type->flags |= TYPE_FLAG_VOLATILE;
		else if (token_is_keyword(top, KWD_TYPE_RESTRICT)) 
			type->flags |= TYPE_FLAG_RESTRICT;
		else
			break;

		toklist_remove_last(stack);
	}
}


static struct ast_node *parse_decl_part(struct parser *parser, struct toklist *stack)
{
	struct ast_node *node;
	struct token *top;

	node = ast_node_new(&parser->ctx);
	node->node_type = AST_NODE_TYPE;

again:
	parser_parse_type_qual(parser, node, stack);

	if (token_is(parser->token, TOKEN_RPAREN) || token_is(parser->token, TOKEN_SEMICOLON)) {
		top = toklist_remove_last(stack);

		if (top->type == TOKEN_ASTERISK) {
			node->ctype = CTYPE_PTR;
		}
		else if (top->type == TOKEN_LPAREN) {
			parser_next(parser);
			goto again;
		}
		else if (token_is_keyword(top, KWD_TYPE_INT)) {
			node->ctype = CTYPE_INT;
			parser_parse_type_qual(parser, node, stack);
		}
		else {
			DEBUG_PRINTF("Unexpected token: %s\n", token_to_string(top));
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

	return node;
}


/*
 * Examples of various declarations:
 *
 *     int a
 *     int * const *b[]
 *     const int * const c
 *     volatile int *d[][]
 *     int *(*f(int a))
 *
 */
static void parse_decl_do(struct parser *parser, struct ast_node *parent, struct toklist *stack)
{
	struct ast_node *prev = parent;
	struct ast_node *child;

	while (true) {
		if (prev->node_type == AST_NODE_TYPE && prev->ctype == CTYPE_INT)
			break;

		if (token_is(parser->token, TOKEN_SEMICOLON) && toklist_is_empty(stack))
			break;

		child = parse_decl_part(parser, stack);
		prev->type = child;
		prev = child;
	}
}


struct ast_node *parser_parse_decl(struct parser *parser)
{
	struct toklist stack;
	struct ast_node *decl;
	
	toklist_init(&stack);

	/*
	 * Put tokens onto stack until you run into the name which is being declared.
	 */
	while ((!token_is_name(parser->token) || token_is_any_keyword(parser->token))
		&& !token_is_eof(parser->token) && !token_is(parser->token, TOKEN_SEMICOLON)) {
		toklist_insert_last(&stack, parser->token);
		parser_next(parser);
	}

	parser_skip(parser); /* name */

	decl = ast_node_new(&parser->ctx);
	decl->node_type = AST_NODE_DECL;
	parse_decl_do(parser, decl, &stack);

	toklist_free(&stack);

	return decl;
}


void dump_decl(struct ast_node *decl)
{
	assert(decl->node_type == AST_NODE_TYPE);

	if (decl->flags & TYPE_FLAG_CONST)
		fprintf(stderr, "constant ");

	if (decl->flags & TYPE_FLAG_RESTRICT)
		fprintf(stderr, "restrict ");

	if (decl->flags & TYPE_FLAG_VOLATILE)
		fprintf(stderr, "volatile ");

	switch (decl->ctype) {
	case CTYPE_PTR:
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
}


void parser_build_ast(struct parser *parser, struct ast *tree, char *cfile)
{
	struct ast_node *decl;

	cpp_open_file(parser->cpp, cfile);
	parser_next(parser);
	decl = parser_parse_decl(parser);
	cpp_close_file(parser->cpp);

	dump_decl(decl->type);
	fprintf(stderr, "\n");
}
