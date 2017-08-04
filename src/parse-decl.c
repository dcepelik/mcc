#include "keyword.h"
#include "array.h"
#include "parse-internal.h"


static struct ast_node *parse_declarators(struct parser *parser, struct toklist *stack);


/*
 * Parses: 6.7.6 pointer
 *
 * Only parses a single pointer (without the right recursion, which is contained in
 * parse_declarators).
 */
static struct ast_node *parse_pointer(struct parser *parser, struct toklist *stack)
{
	enum tqual tquals;
	struct token *top;
	struct ast_node *decl = NULL;

	TMP_ASSERT(!toklist_is_empty(stack));
	top = toklist_remove_last(stack);

	tquals = 0;
	while (token_is_tqual(top)) {
		tquals |= top->symbol->def->keyword->tqual;
		TMP_ASSERT(!toklist_is_empty(stack));
		top = toklist_remove_last(stack);
	}

	TMP_ASSERT(token_is(top, TOKEN_ASTERISK));
	decl = ast_node_new(&parser->ctx, AST_POINTER);
	decl->tquals = tquals;
	decl->decl = parse_declarators(parser, stack);
	return decl;
}


/*
 * Parses: 6.7.6 declarator, 6.7.6 direct-declarator
 */
static struct ast_node *parse_declarators(struct parser *parser, struct toklist *stack)
{
	struct ast_node *decl = NULL;

	if (token_is(parser->token, TOKEN_RPAREN)) {
		TMP_ASSERT(!toklist_is_empty(stack));
		if (token_is(toklist_last(stack), TOKEN_LPAREN)) {
			toklist_remove_last(stack); /* skip `(' on the left */
			parser_next(parser); /* skip `)' on the right */
			return parse_declarators(parser, stack);
		}

		return parse_pointer(parser, stack);
	}
	else if (token_is(parser->token, TOKEN_LBRACKET)) {
		decl = ast_node_new(&parser->ctx, AST_ARRAY);
		parser_next(parser);
		if (parser->token->type == TOKEN_NUMBER)
			parser_next(parser); /* TODO */
		TMP_ASSERT(token_is(parser->token, TOKEN_RBRACKET));
		parser_next(parser);
		decl->decl = parse_declarators(parser, stack);
		return decl;
	}
	else if (token_is(parser->token, TOKEN_SEMICOLON) || token_is(parser->token, TOKEN_COMMA)) {
		if (!toklist_is_empty(stack))
			return parse_pointer(parser, stack);
		return NULL;
	}
	else {
		token_dump(parser->token, stderr);
		assert(false);
	}
}


/*
 * Parses: 6.7 init-declarator.
 */
static struct ast_node *parse_init_declarator(struct parser *parser)
{
	struct ast_node *part;
	struct toklist stack;

	part = ast_node_new(&parser->ctx, AST_DECL_PART);
	
	/* now push all leading *, ( and type qualifiers onto the stack */
	toklist_init(&stack);
	while (token_is(parser->token, TOKEN_ASTERISK)
		|| token_is(parser->token, TOKEN_LPAREN)
		|| token_is_tqual(parser->token)) {
		toklist_insert_last(&stack, parser->token);
		parser_next(parser);
	}

	TMP_ASSERT(token_is_name(parser->token) && !token_is_any_keyword(parser->token));
	part->ident = symbol_get_name(parser->token->symbol);
	parser_next(parser);
	part->decl = parse_declarators(parser, &stack);
	toklist_free(&stack);

	return part;
}


/*
 * Parses: 6.7 declaration-specifiers.
 */
static void parse_declspec(struct parser *parser, struct ast_node *decln)
{
	const struct kwd *kwd;

	while (token_is_any_keyword(parser->token)) {
		kwd = parser->token->symbol->def->keyword;

		switch (kwd->class) {
			case KWD_CLASS_ALIGNMENT:
				break;
			case KWD_CLASS_TSPEC:
				decln->tspec |= kwd->tspec;
				break;
			case KWD_CLASS_FUNCSPEC:
				break;
			case KWD_CLASS_STORCLS:
				decln->storcls |= kwd->storcls;
				break;
			case KWD_CLASS_TQUAL:
				decln->tquals |= kwd->tqual;
				break;
			default:
				return;
		}

		parser_next(parser);
	}
}


/*
 * Parses: 6.7 declaration, 6.7 init-declarator-list.
 */
struct ast_node *parser_parse_decl(struct parser *parser)
{
	struct ast_node *decl;
	bool comma = false;

	decl = ast_node_new(&parser->ctx, AST_DECL);
	parse_declspec(parser, decl);
	decl->parts = array_new(1, sizeof(*decl->parts));

	while (!token_is_eof(parser->token)) {
		if (token_is(parser->token, TOKEN_SEMICOLON)) {
			TMP_ASSERT(!comma);
			parser_next(parser);
			break;
		}

		if (array_size(decl->parts) > 0) {
			TMP_ASSERT(token_is(parser->token, TOKEN_COMMA));
			comma = true;
			parser_next(parser);
		}

		decl->parts = array_claim(decl->parts, 1);
		decl->parts[array_size(decl->parts) - 1] = parse_init_declarator(parser);

		comma = false;
	}

	return decl;
}


static void print_declspec(struct ast_node *node, struct strbuf *buf)
{
	if (node->tquals & TQUAL_CONST)
		strbuf_printf(buf, "const ");
	if (node->tquals & TQUAL_RESTRICT)
		strbuf_printf(buf, "restrict ");
	if (node->tquals & TQUAL_VOLATILE)
		strbuf_printf(buf, "volatile ");
	if (node->tspec & TSPEC_INT)
		strbuf_printf(buf, "int ");
}


static void print_init_declarator(struct ast_node *part, struct ast_node *decl, struct strbuf *buf)
{
	bool need_parens = false;
	struct strbuf decl_buf;
	struct strbuf declspec_buf;

	strbuf_init(&decl_buf, 8);
	strbuf_init(&declspec_buf, 16);

	strbuf_printf(&decl_buf, part->ident);
	while (decl) {
		switch (decl->type) {
			case AST_ARRAY:
				if (need_parens) {
					strbuf_prepend(&decl_buf, "(");
					strbuf_printf(&decl_buf, ")[]");
				}
				else {
					strbuf_printf(&decl_buf, "[]");
				}
				need_parens = false;
				break;
			case AST_POINTER:
				strbuf_reset(&declspec_buf);
				print_declspec(decl, &declspec_buf);
				if (strbuf_strlen(&declspec_buf) > 0)
					strbuf_prepend(&decl_buf, "* %s", strbuf_get_string(&declspec_buf));
				else
					strbuf_prepend(&decl_buf, "*");
				need_parens = true;
				break;
			default:
				TMP_ASSERT(false);
		}

		decl = decl->decl;
	}

	strbuf_printf(buf, strbuf_get_string(&decl_buf));
	strbuf_free(&decl_buf);
}


void dump_decln(struct ast_node *decln)
{
	size_t i;

	struct strbuf buf;
	strbuf_init(&buf, 64);

	TMP_ASSERT(decln->type == AST_DECL);

	print_declspec(decln, &buf);

	for (i = 0; i < array_size(decln->parts); i++) {
		print_init_declarator(decln->parts[i], decln->parts[i]->decl, &buf);
		if (i < array_size(decln->parts) - 1)
			strbuf_printf(&buf, ", ");
	}
	strbuf_printf(&buf, ";");

	fprintf(stderr, strbuf_get_string(&buf));
	strbuf_free(&buf);
}
