#include "array.h"
#include "keyword.h"
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
		if (parser->token->type == TOKEN_RBRACKET)
			decl->size_expr = NULL;
		else
			decl->size_expr = parse_expr(parser);
		parser_require(parser, TOKEN_RBRACKET);
		decl->decl = parse_declarators(parser, stack);
		return decl;
	}
	else if (token_is(parser->token, TOKEN_SEMICOLON)
		|| token_is(parser->token, TOKEN_COMMA)
		|| token_is(parser->token, TOKEN_OP_ASSIGN)) {
		if (!toklist_is_empty(stack))
			return parse_pointer(parser, stack);
		return NULL;
	}
	else {
		parse_error(parser, "unexpected %s, one of [;,)= was expected",
			token_get_name(parser->token->type));
		return NULL;
	}
}


/*
 * Parses: 6.7 init-declarator.
 */
static struct ast_init_declr *parse_init_declr(struct parser *parser)
{
	struct ast_declr *declr;
	struct toklist stack;

	part = ast_node_new(&parser->ctx, AST_DECL_PART);
	
	/* push all leading *, ( and type qualifiers onto the stack */
	toklist_init(&stack);
	while (token_is(parser->token, TOKEN_ASTERISK)
		|| token_is(parser->token, TOKEN_LPAREN)
		|| token_is_tqual(parser->token)) {
		toklist_insert_last(&stack, parser->token);
		parser_next(parser);
	}

	if (!token_is_name(parser->token) || token_is_any_keyword(parser->token)) {
		parse_error(parser, "identifier was expected");
		assert(0);
	}
	part->ident = symbol_get_name(parser->token->symbol);
	parser_next(parser);
	part->decl = parse_declarators(parser, &stack);
	toklist_free(&stack);

	part->init = NULL;
	if (token_is(parser->token, TOKEN_OP_ASSIGN)) {
		parser_next(parser);
		if (token_is(parser->token, TOKEN_LBRACE))
			assert(0);
		else
			part->init = parse_expr(parser);
	}

	return part;
}


static void sanitize_dspec_su_member(struct parser *parser, struct ast_node_2 *member)
{
	assert(member->type == AST_SU_SPEC);

	if (member->decl.declspec.storcls) {
		parse_error(parser, "Storage class specifier not valid for struct "
			"and union members.");
		member->decl.declspec.storcls = 0;
	}
}


/*
 * Parses: 6.7.2.1 struct-or-union-specifier.
 */
static struct ast_node_2 *parse_su_spec(struct parser *parser)
{
	assert(token_is_keyword(parser->token, KWD_STRUCT) || token_is_keyword(parser->token, KWD_UNION));
	parser_next(parser);

	struct ast_node_2 *node;
	struct ast_su_spec *spec;
	struct ast_node_2 *member;
	
	node = ast_node_2_new(&parser->ctx, AST_SU_SPEC);
	spec = &node->su_spec;
	spec->members = array_new(2, sizeof(*spec->members));

	if (token_is_name(parser->token) && !token_is_any_keyword(parser->token)) {
		spec->name = symbol_get_name(parser->token->symbol);
		parser_next(parser);
	}
	else {
		spec->name = NULL; /* anonymous struct */
	}

	if (!parser_expect(parser, TOKEN_LBRACE)) {
		if (!spec->name)
			parse_error(parser, "struct name expected");
		return node;
	}

	while (!token_is_eof(parser->token) && !token_is(parser->token, TOKEN_RBRACE)) {
		member = parser_parse_decl(parser);
		sanitize_dspec_su_member(parser, member);
		array_push(spec->members, member);
	}

	parser_require(parser, TOKEN_RBRACE);
	return node;
}


static void apply_tflag(struct parser *parser,
                        struct ast_declspec *dspec,
			const struct kwdinfo *kwd)
{
	uint8_t new_tflags = dspec->tflags;

	switch (kwd->tflags) {
	case TFLAG_LONG:
		if (dspec->tflags & TFLAG_LONG) { /* promote long to long long */
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
	else if (new_tflags == dspec->tflags)
		parse_error(parser, "Type specifier is redundant.");
	else
		dspec->tflags = new_tflags;
}


static void apply_tspec(struct parser *parser,
                        struct ast_declspec *dspec,
			const struct kwdinfo *kwd)
{
	if (dspec->tspec)
		parse_error(parser, "Too many types specified.");
	else
		dspec->tspec |= kwd->tspec;
}


static void apply_storcls(struct parser *parser,
                          struct ast_declspec *dspec,
			  const struct kwdinfo *kwd)
{
	if (dspec->storcls)
		parse_error(parser, "Too many storage classes specified.");
	else
		dspec->storcls |= kwd->storcls;
}


static void sanitize_declspec(struct parser *parser, struct ast_declspec *dspec)
{
	if (!dspec->tspec) {
		if (!(dspec->tflags & INT_TFLAGS))
			parse_error(parser, "Type not specified, assume it's an int.");
		dspec->tspec = TSPEC_INT;
	}

	switch (dspec->tspec) {
	case TSPEC_INT:
		if (dspec->tflags & ~INT_TFLAGS) {
			parse_error(parser, "Invalid type specifiers for int, ignore.");
			dspec->tflags &= INT_TFLAGS;
		};
		break;

	case TSPEC_CHAR:
		if (dspec->tflags & ~CHAR_TFLAGS) {
			parse_error(parser, "Invalid type specifiers for char, ingore.");
			dspec->tflags &= CHAR_TFLAGS;
		}
		break;

	case TSPEC_BOOL:
		if (dspec->tflags) {
			parse_error(parser, "Invalid type specifiers for _Bool, ignore.");
			dspec->tflags = 0;
		}
		break;

	case TSPEC_FLOAT:
		if (dspec->tflags) {
			parse_error(parser, "Invalid type specifiers for float, ignore.");
			dspec->tflags = 0;
		}
		break;

	case TSPEC_DOUBLE:
		if (dspec->tflags & ~DOUBLE_TFLAGS) {
			parse_error(parser, "Invalid type specifiers for double, ignore.");
			dspec->tflags &= DOUBLE_TFLAGS;
		}
	}

	if (dspec->tflags & TFLAG_COMPLEX
		&& dspec->tspec != TSPEC_FLOAT && dspec->tspec != TSPEC_DOUBLE) {
		parse_error(parser, "Only float and double can be _Complex.");
	}
}


/*
 * Parses: 6.7 declaration-specifiers.
 */
static void parse_declspec(struct parser *parser, struct ast_declspec *dspec)
{
	const struct kwdinfo *kwdinfo;

	dspec->tspec = 0;
	dspec->tflags = 0;
	dspec->tquals = 0;
	dspec->storcls = 0;

	while (token_is_any_keyword(parser->token)) {
		kwdinfo = parser->token->symbol->def->kwdinfo;

		switch (kwdinfo->class) {
			case KWD_CLASS_ALIGNMENT:
				break;
			case KWD_CLASS_TSPEC:
				apply_tspec(parser, dspec, kwdinfo);
				break;
			case KWD_CLASS_TFLAG:
				apply_tflag(parser, dspec, kwdinfo);
				break;
			case KWD_CLASS_FUNCSPEC:
				break;
			case KWD_CLASS_STORCLS:
				apply_storcls(parser, dspec, kwdinfo);
				break;
			case KWD_CLASS_TQUAL:
				dspec->tquals |= kwdinfo->tqual;
				break;
			default:
				goto break_while;
		}

		if (kwdinfo->kwd == KWD_STRUCT || kwdinfo->kwd == KWD_UNION)
			dspec->su_spec = parse_su_spec(parser);
		else
			parser_next(parser);
	}

break_while:
	sanitize_declspec(parser, dspec);
}


/*
 * Parses: 6.7 declaration, 6.7 init-declarator-list.
 */
struct ast_node_2 *parser_parse_decl(struct parser *parser)
{
	struct ast_node_2 *node;
	struct ast_decl *decl;
	struct ast_init_declr *declr;
	bool comma = false;

	node = ast_node_2_new(&parser->ctx, AST_DECL);
	decl = &node->decl;
	list_init(&decl->init_decl_list);
	parse_declspec(parser, &decl->declspec);

	while (!token_is_eof(parser->token)) {
		if (token_is(parser->token, TOKEN_SEMICOLON)) {
			if (comma)
				parse_error(parser, "trailing `,' not allowed here, ignored");
			parser_next(parser);
			break;
		}

		if (!comma && list_length(&decl->init_decl_list) > 0)
			parse_error(parser, "`,' was expected");

		declr = parse_init_declr(parser);
		list_insert_last(&decl->init_decl_list, &declr->node);

		comma = false;
		if (token_is(parser->token, TOKEN_COMMA)) {
			comma = true;
			parser_next(parser);
		}
	}

	return node;
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


static void print_init_declarator(struct ast_node *part,
                                  struct ast_node *decl,
				  struct strbuf *buf)
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
					strbuf_printf(&decl_buf, ")");
				}

				strbuf_printf(&decl_buf, "[");
				if (decl->size_expr)
					dump_expr(decl->size_expr, &decl_buf);
				strbuf_printf(&decl_buf, "]");
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

	if (part->init) {
		strbuf_printf(&decl_buf, " = ");
		dump_expr(part->init, &decl_buf);
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
	/*struct strbuf buf;
	strbuf_init(&buf, 64);

	TMP_ASSERT(decln->type == AST_DECL);
	print_decln(decln, &buf);

	fprintf(stderr, strbuf_get_string(&buf));
	strbuf_free(&buf);*/
}
