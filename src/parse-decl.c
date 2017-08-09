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
		tquals |= top->symbol->def->kwdinfo->tqual;
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
		parser_require(parser, TOKEN_RBRACKET);
		decl->decl = parse_declarators(parser, stack);
		return decl;
	}
	else if (token_is(parser->token, TOKEN_SEMICOLON) || token_is(parser->token, TOKEN_COMMA)) {
		if (!toklist_is_empty(stack))
			return parse_pointer(parser, stack);
		return NULL;
	}
	else {
		parse_error(parser, "unexpected %s, one of [;,) was expected",
			token_get_name(parser->token->type));
		return NULL;
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


static void sanitize_declspec_member(struct parser *parser, struct ast_node *decln)
{
	if (decln->storcls) {
		parse_error(parser, "Storage class specifier not valid for struct "
			"and union members.");
		decln->storcls = 0;
	}
}


/*
 * Parses: 6.7.2.1 struct-or-union-specifier.
 */
static struct ast_node *parse_struct_or_union_specifier(struct parser *parser)
{
	struct ast_node *spec;

	if (token_is_keyword(parser->token, KWD_STRUCT))
		spec = ast_node_new(&parser->ctx, AST_STRUCT_SPEC);
	else if (token_is_keyword(parser->token, KWD_UNION))
		spec = ast_node_new(&parser->ctx, AST_UNION_SPEC);
	else
		assert(0);

	parser_next(parser);
	spec->ident = NULL;
	spec->decls = NULL;

	if (token_is_name(parser->token) && !token_is_any_keyword(parser->token)) {
		spec->ident = symbol_get_name(parser->token->symbol);
		parser_next(parser);
	}

	if (!parser_expect(parser, TOKEN_LBRACE)) {
		if (!spec->ident)
			parse_error(parser, "struct name expected");
		return spec;
	}

	spec->decls = array_new(2, sizeof(*spec->decls));
	while (!token_is_eof(parser->token)) {
		if (token_is(parser->token, TOKEN_RBRACE))
			break;

		spec->decls = array_claim(spec->decls, 1);
		spec->decls[array_size(spec->decls) - 1] = parser_parse_decl(parser);
		sanitize_declspec_member(parser, spec->decls[array_size(spec->decls) - 1]);
	}

	parser_require(parser, TOKEN_RBRACE);
	return spec;
}


static void apply_tflag(struct parser *parser, struct ast_node *decln, const struct kwdinfo *kwd)
{
	uint8_t new_tflags = decln->tflags;

	switch (kwd->tflags) {
	case TFLAG_LONG:
		if (decln->tflags & TFLAG_LONG) { /* promote long to long long */
			new_tflags ^= TFLAG_LONG;
			new_tflags |= TFLAG_LONG_LONG;
			break;
		}

		/* fall-through */

	default:
		new_tflags |= kwd->tflags;
	}

	if (new_tflags & TFLAG_SIGNED && new_tflags & TFLAG_UNSIGNED)
		parse_error(parser, "Both signed and unsigned? What do the doctors say?");
	else if (new_tflags & (TFLAG_LONG | TFLAG_LONG_LONG) && new_tflags & TFLAG_SHORT)
		parse_error(parser, "Both long and short? What would that be?");
	else if (new_tflags & TFLAG_LONG && new_tflags & TFLAG_LONG_LONG)
		parse_error(parser, "Longer than long long? Have a break.");
	else if (new_tflags & TFLAG_COMPLEX && new_tflags & ~(TFLAG_LONG | TFLAG_COMPLEX))
		parse_error(parser, "Invalid type specifier for _Complex.");
	else if (new_tflags == decln->tflags)
		parse_error(parser, "Type specifier is redundant.");
	else
		decln->tflags = new_tflags;
}


static void apply_tspec(struct parser *parser, struct ast_node *decln, const struct kwdinfo *kwd)
{
	if (decln->tspec)
		parse_error(parser, "Too many types specified.");
	else
		decln->tspec |= kwd->tspec;
}


static void apply_storcls(struct parser *parser, struct ast_node *decln, const struct kwdinfo *kwd)
{
	if (decln->storcls)
		parse_error(parser, "Too many storage classes specified.");
	else
		decln->storcls |= kwd->storcls;
}


static void sanitize_declspec(struct parser *parser, struct ast_node *decln)
{
	if (!decln->tspec) {
		if (!(decln->tflags & INT_TFLAGS))
			parse_error(parser, "Type not specified, assume it's an int.");
		decln->tspec = TSPEC_INT;
	}

	switch (decln->tspec) {
	case TSPEC_INT:
		if (decln->tflags & ~INT_TFLAGS) {
			parse_error(parser, "Invalid type specifiers for int, ignore.");
			decln->tflags &= INT_TFLAGS;
		};
		break;

	case TSPEC_CHAR:
		if (decln->tflags & ~CHAR_TFLAGS) {
			parse_error(parser, "Invalid type specifiers for char, ingore.");
			decln->tflags &= CHAR_TFLAGS;
		}
		break;

	case TSPEC_BOOL:
		if (decln->tflags) {
			parse_error(parser, "Invalid type specifiers for _Bool, ignore.");
			decln->tflags = 0;
		}
		break;

	case TSPEC_FLOAT:
		if (decln->tflags) {
			parse_error(parser, "Invalid type specifiers for float, ignore.");
			decln->tflags = 0;
		}
		break;

	case TSPEC_DOUBLE:
		if (decln->tflags & ~DOUBLE_TFLAGS) {
			parse_error(parser, "Invalid type specifiers for double, ignore.");
			decln->tflags &= DOUBLE_TFLAGS;
		}
	}

	if (decln->tflags & TFLAG_COMPLEX
		&& decln->tspec != TSPEC_FLOAT && decln->tspec != TSPEC_DOUBLE) {
		parse_error(parser, "Only float and double can be _Complex.");
	}
}


/*
 * Parses: 6.7 declaration-specifiers.
 */
static void parse_declspec(struct parser *parser, struct ast_node *decln)
{
	const struct kwdinfo *kwdinfo;

	decln->tspec = 0;
	decln->tflags = 0;
	decln->tquals = 0;
	decln->storcls = 0;

	while (token_is_any_keyword(parser->token)) {
		kwdinfo = parser->token->symbol->def->kwdinfo;

		switch (kwdinfo->class) {
			case KWD_CLASS_ALIGNMENT:
				break;

			case KWD_CLASS_TSPEC:
				apply_tspec(parser, decln, kwdinfo);
				break;

			case KWD_CLASS_TFLAG:
				apply_tflag(parser, decln, kwdinfo);
				break;

			case KWD_CLASS_FUNCSPEC:
				break;

			case KWD_CLASS_STORCLS:
				apply_storcls(parser, decln, kwdinfo);
				break;

			case KWD_CLASS_TQUAL:
				decln->tquals |= kwdinfo->tqual;
				break;

			default:
				goto out;
		}

		if (kwdinfo->kwd == KWD_STRUCT || kwdinfo->kwd == KWD_UNION)
			decln->spec = parse_struct_or_union_specifier(parser);
		else
			parser_next(parser);
	}
out:
	sanitize_declspec(parser, decln);
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
			parser_require(parser, TOKEN_COMMA);
			comma = true;
		}

		decl->parts = array_claim(decl->parts, 1);
		decl->parts[array_size(decl->parts) - 1] = parse_init_declarator(parser);

		comma = false;
	}

	return decl;
}


void print_decln(struct ast_node *decln, struct strbuf *buf);


static const char *tspec_to_string(enum tspec tspec)
{
	switch (tspec) {
	case TSPEC_BOOL:
		return "_Bool";
	case TSPEC_CHAR:
		return "char";
	case TSPEC_DOUBLE:
		return "double";
	case TSPEC_ENUM:
		return "enum";
	case TSPEC_FLOAT:
		return "float";
	case TSPEC_INT:
		return "int";
	case TSPEC_STRUCT:
		return "struct";
	case TSPEC_UNION:
		return "union";
	case TSPEC_VOID:
		return "void";
	default:
		assert(0);
	}
}


static const char *tqual_to_string(enum tqual tqual)
{
	switch (tqual) {
	case TQUAL_CONST:
		return "const";
	case TQUAL_RESTRICT:
		return "restrict";
	case TQUAL_VOLATILE:
		return "volatile";
	case TQUAL_ATOMIC:
		return "_Atomic";
	default:
		assert(0);
	}
}


static const char *tflag_to_string(enum tflag tflag)
{
	switch (tflag) {
	case TFLAG_UNSIGNED:
		return "unsigned";
	case TFLAG_SIGNED:
		return "signed";
	case TFLAG_SHORT:
		return "short";
	case TFLAG_LONG:
		return "long";
	case TFLAG_LONG_LONG:
		return "long long";
	case TFLAG_COMPLEX:
		return "_Complex";
	default:
		assert(0);
	}
}


static void print_declspec(struct ast_node *node, struct strbuf *buf)
{
	size_t i;

	if (node->tquals & TQUAL_CONST)
		strbuf_printf(buf, "const ");
	if (node->tquals & TQUAL_RESTRICT)
		strbuf_printf(buf, "restrict ");
	if (node->tquals & TQUAL_VOLATILE)
		strbuf_printf(buf, "volatile ");

	if (node->tflags & TFLAG_UNSIGNED)
		strbuf_printf(buf, "unsigned ");
	else if (node->tflags & TFLAG_SIGNED)
		strbuf_printf(buf, "signed ");
	
	if (node->tflags & TFLAG_SHORT)
		strbuf_printf(buf, "short ");
	else if (node->tflags & TFLAG_LONG)
		strbuf_printf(buf, "long ");
	else if (node->tflags & TFLAG_LONG_LONG)
		strbuf_printf(buf, "long long ");
	
	if (node->tflags & TFLAG_COMPLEX)
		strbuf_printf(buf, "_Complex ");

	strbuf_printf(buf, "%s ", tspec_to_string(node->tspec));

	if (node->tspec & TSPEC_STRUCT) {
		if (node->spec->ident)
			strbuf_printf(buf, "struct %s ", node->spec->ident);
		else
			strbuf_printf(buf, "struct ");

		if (node->spec->decls) {
			strbuf_printf(buf, "{\n");
			for (i = 0; i < array_size(node->spec->decls); i++) {
				strbuf_printf(buf, "\t");
				print_decln(node->spec->decls[i], buf);
				strbuf_printf(buf, "\n");
			}
			strbuf_printf(buf, "} ");
		}
	}
	if (node->tspec & TSPEC_UNION) {
		strbuf_printf(buf, "union ");
	}
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


void print_decln(struct ast_node *decln, struct strbuf *buf)
{
	size_t i;

	print_declspec(decln, buf);

	for (i = 0; i < array_size(decln->parts); i++) {
		print_init_declarator(decln->parts[i], decln->parts[i]->decl, buf);
		if (i < array_size(decln->parts) - 1)
			strbuf_printf(buf, ", ");
	}
	strbuf_printf(buf, ";");
}


void dump_decln(struct ast_node *decln)
{
	struct strbuf buf;
	strbuf_init(&buf, 64);

	TMP_ASSERT(decln->type == AST_DECL);
	print_decln(decln, &buf);

	fprintf(stderr, strbuf_get_string(&buf));
	strbuf_free(&buf);
}
