#include "parser.h"
#include "keyword.h"


static const struct {
	char *name;
	enum c_keyword keyword;
}
keywords[] = {
	{ .name = "auto", .keyword = C_KEYWORD_AUTO },
	{ .name = "break", .keyword = C_KEYWORD_BREAK },
	{ .name = "case", .keyword = C_KEYWORD_CASE },
	{ .name = "char", .keyword = C_KEYWORD_CHAR },
	{ .name = "const", .keyword = C_KEYWORD_CONST },
	{ .name = "continue", .keyword = C_KEYWORD_CONTINUE },
	{ .name = "default", .keyword = C_KEYWORD_DEFAULT },
	{ .name = "do",  .keyword = C_KEYWORD_DO },
	{ .name = "double", .keyword = C_KEYWORD_DOUBLE },
	{ .name = "else", .keyword = C_KEYWORD_ELSE },
	{ .name = "enum", .keyword = C_KEYWORD_ENUM },
	{ .name = "extern", .keyword = C_KEYWORD_EXTERN },
	{ .name = "float", .keyword = C_KEYWORD_FLOAT },
	{ .name = "for", .keyword = C_KEYWORD_FOR },
	{ .name = "goto", .keyword = C_KEYWORD_GOTO },
	{ .name = "if",  .keyword = C_KEYWORD_IF },
	{ .name = "inline", .keyword = C_KEYWORD_INLINE },
	{ .name = "int", .keyword = C_KEYWORD_INT },
	{ .name = "long", .keyword = C_KEYWORD_LONG },
	{ .name = "register", .keyword = C_KEYWORD_REGISTER },
	{ .name = "restrict", .keyword = C_KEYWORD_RESTRICT },
	{ .name = "return", .keyword = C_KEYWORD_RETURN },
	{ .name = "short", .keyword = C_KEYWORD_SHORT },
	{ .name = "signed", .keyword = C_KEYWORD_SIGNED },
	{ .name = "sizeof", .keyword = C_KEYWORD_SIZEOF },
	{ .name = "static", .keyword = C_KEYWORD_STATIC },
	{ .name = "struct", .keyword = C_KEYWORD_STRUCT },
	{ .name = "switch", .keyword = C_KEYWORD_SWITCH },
	{ .name = "typedef", .keyword = C_KEYWORD_TYPEDEF },
	{ .name = "union", .keyword = C_KEYWORD_UNION },
	{ .name = "unsigned", .keyword = C_KEYWORD_UNSIGNED },
	{ .name = "void", .keyword = C_KEYWORD_VOID },
	{ .name = "volatile", .keyword = C_KEYWORD_VOLATILE },
	{ .name = "while", .keyword = C_KEYWORD_WHILE },
	{ .name = "_Alignas", .keyword = C_KEYWORD_ALIGNAS },
	{ .name = "_Alignof", .keyword = C_KEYWORD_ALIGNOF },
	{ .name = "_Atomic", .keyword = C_KEYWORD_ATOMIC },
	{ .name = "_Bool", .keyword = C_KEYWORD_BOOL },
	{ .name = "_Complex", .keyword = C_KEYWORD_COMPLEX },
	{ .name = "_Generic", .keyword = C_KEYWORD_GENERIC },
	{ .name = "_Imaginary", .keyword = C_KEYWORD_IMAGINARY },
	{ .name = "_Noreturn", .keyword = C_KEYWORD_NORETURN },
	{ .name = "_Static_assert", .keyword = C_KEYWORD_STATIC_ASSERT },
	{ .name = "_Thread_local", .keyword = C_KEYWORD_THREAD_LOCAL },
};


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
	parser_next(parser);
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
		def->keyword = keywords[i].keyword;
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

		if (token_is_keyword(top, C_KEYWORD_CONST))
			type->flags |= TYPE_FLAG_CONST;
		else if (token_is_keyword(top, C_KEYWORD_VOLATILE))
			type->flags |= TYPE_FLAG_VOLATILE;
		else if (token_is_keyword(top, C_KEYWORD_RESTRICT)) 
			type->flags |= TYPE_FLAG_RESTRICT;
		else
			break;

		toklist_remove_last(stack);
	}
}


static struct ast_node *parser_parse_declaration_part(struct parser *parser, struct toklist *stack)
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
			node->ctype = CTYPE_POINTER;
		}
		else if (top->type == TOKEN_LPAREN) {
			parser_next(parser);
			goto again;
		}
		else if (token_is_keyword(top, C_KEYWORD_INT)) {
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
static void parser_parse_declaration_do(struct parser *parser, struct ast_node *parent, struct toklist *stack)
{
	struct ast_node *prev = parent;
	struct ast_node *child;

	while (true) {
		if (prev->node_type == AST_NODE_TYPE && prev->ctype == CTYPE_INT)
			break;

		if (token_is(parser->token, TOKEN_SEMICOLON) && toklist_is_empty(stack))
			break;

		child = parser_parse_declaration_part(parser, stack);
		prev->type = child;
		prev = child;
	}
}


struct ast_node *parser_parse_declaration(struct parser *parser)
{
	struct toklist stack;
	struct ast_node *decl;
	
	toklist_init(&stack);

	while ((!token_is_name(parser->token)
		|| token_is_keyword(parser->token, C_KEYWORD_CONST)
		|| token_is_keyword(parser->token, C_KEYWORD_RESTRICT)
		|| token_is_keyword(parser->token, C_KEYWORD_VOLATILE)
		|| token_is_keyword(parser->token, C_KEYWORD_INT))
		&& !token_is_eof(parser->token)) {
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

	if (decl->flags & TYPE_FLAG_CONST)
		fprintf(stderr, "constant ");

	if (decl->flags & TYPE_FLAG_RESTRICT)
		fprintf(stderr, "restrict ");

	if (decl->flags & TYPE_FLAG_VOLATILE)
		fprintf(stderr, "volatile ");

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
}


void parser_build_ast(struct parser *parser, struct ast *tree, char *cfile)
{
	struct ast_node *decl;

	cpp_open_file(parser->cpp, cfile);
	parser_next(parser);
	decl = parser_parse_declaration(parser);
	cpp_close_file(parser->cpp);

	dump_decl(decl->type);
	fprintf(stderr, "\n");
}
