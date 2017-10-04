#include "array.h"
#include "keyword.h"
#include "parse-internal.h"

#define ANON_STRUCT_NAME	"<anonymous>"

/*
 * Parses: 6.7.6 pointer
 *
 * NOTE: Only parses a single pointer declarator (without the right recursion,
 *       which is contained in parse_declrs).
 */
static void parse_ptr_declr(struct parser *parser,
                            struct toklist *stack,
			    struct ast_declr *declr)
{
	(void) parser;

	enum tqual tquals;
	struct token *top;

	TMP_ASSERT(!toklist_is_empty(stack));
	top = toklist_remove_last(stack);

	tquals = 0;
	while (token_is_tqual(top)) {
		tquals |= top->symbol->def->kwdinfo->tqual;
		TMP_ASSERT(!toklist_is_empty(stack));
		top = toklist_remove_last(stack);
	}

	TMP_ASSERT(token_is(top, TOKEN_ASTERISK));
	declr->type = DECLR_TYPE_PTR;
	declr->tquals = tquals;
}

/*
 * Parses: 6.7.6 declarator, 6.7.6 direct-declarator
 */
static void parse_declrs(struct parser *parser,
                         struct toklist *stack,
			 struct ast_init_declr *init_declr)
{
	struct ast_declr *declr;

	while (true) {
		if (token_is(parser->token, TOKEN_RPAREN)) {
			TMP_ASSERT(!toklist_is_empty(stack));
			if (token_is(toklist_last(stack), TOKEN_LPAREN)) {
				toklist_remove_last(stack); /* skip `(' on the left */
				parser_next(parser); /* skip `)' on the right */
				continue;
			}

			parse_ptr_declr(parser, stack, array_push_new(init_declr->declrs));
		}
		else if (token_is(parser->token, TOKEN_LBRACKET)) {
			parser_next(parser);
			declr = array_push_new(init_declr->declrs);
			declr->type = DECLR_TYPE_ARRAY;
			declr->size = parser->token->type == TOKEN_RBRACKET
				? NULL
				: parse_expr(parser);
			parser_require(parser, TOKEN_RBRACKET);
		}
		else if (token_is(parser->token, TOKEN_SEMICOLON)
			|| token_is(parser->token, TOKEN_COMMA)
			|| token_is(parser->token, TOKEN_OP_ASSIGN)) {
			if (toklist_is_empty(stack))
				break;
			parse_ptr_declr(parser, stack, array_push_new(init_declr->declrs));
		}
		else {
			parse_error(parser, "unexpected %s, one of [;,)= was expected",
				token_get_name(parser->token->type));
			break;
		}
	}
}

/*
 * Parses: 6.7 init-declarator.
 */
static void parse_init_declr(struct parser *parser, struct ast_init_declr *init_declr)
{
	struct toklist stack;

	init_declr->declrs = array_new(4, sizeof(*init_declr->declrs));
	
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
		init_declr->ident = NULL;
	}
	else {
		init_declr->ident = symbol_get_name(parser->token->symbol);
	}
	parser_next(parser);
	parse_declrs(parser, &stack, init_declr);
	toklist_free(&stack);

	if (token_is(parser->token, TOKEN_OP_ASSIGN)) {
		parser_next(parser);
		if (token_is(parser->token, TOKEN_LBRACE))
			assert(0);
		else
			init_declr->init = parse_expr(parser);
	}
	else {
		init_declr->init = NULL;
	}
}

static void sanitize_su_member_dspec(struct parser *parser, struct ast_declspec *dspec)
{
	if (dspec->storcls) {
		parse_error(parser, "Storage class specifier not valid for struct "
			"and union members.");
		dspec->storcls = 0;
	}
}

static void parse_decl_internal(struct parser *parser, struct ast_decl *decl);

/*
 * Parses: 6.7.2.1 struct-or-union-specifier.
 */
static struct ast_su_spec *parse_struct_or_union_specifier(struct parser *parser)
{
	assert(token_is_keyword(parser->token, KWD_STRUCT)
		|| token_is_keyword(parser->token, KWD_UNION));

	struct ast_su_spec *spec;

	spec = mcc_malloc(sizeof(*spec)); /* TODO don't malloc individual objects */
	spec->name = NULL;
	spec->members = array_new(4, sizeof(*spec->members));

	parser_next(parser); /* the `struct' or `union' keyword */

	if (token_is_name(parser->token) && !token_is_any_keyword(parser->token)) {
		spec->name = symbol_get_name(parser->token->symbol);
		parser_next(parser);
	}

	if (!parser_expect(parser, TOKEN_LBRACE)) {
		if (!spec->name)
			parse_error(parser, "struct name expected");
		return spec;
	}

	while (!token_is_eof(parser->token) && !token_is(parser->token, TOKEN_RBRACE)) {
		parse_decl_internal(parser, array_push_new(spec->members));
		sanitize_su_member_dspec(parser, &array_last(spec->members).declspec);
	}

	parser_require(parser, TOKEN_RBRACE);
	return spec;
}

static void apply_tflag(struct parser *parser, struct ast_declspec *dspec, const struct kwdinfo *kwd)
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
 *
 * NOTE: This function is used by expression parser to parse cast expressions.
 */
void parse_declspec(struct parser *parser, struct ast_declspec *dspec)
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
			dspec->su_spec = parse_struct_or_union_specifier(parser);
		else
			parser_next(parser);
	}

break_while:
	sanitize_declspec(parser, dspec);
}

/*
 * Parses: 6.7 declaration, 6.7 init-declarator-list.
 */
static void parse_decl_internal(struct parser *parser, struct ast_decl *decl)
{
	bool comma = false;

	decl->init_declrs = array_new(2, sizeof(*decl->init_declrs));
	parse_declspec(parser, &decl->declspec);

	while (!token_is_eof(parser->token)) {
		if (token_is(parser->token, TOKEN_SEMICOLON)) {
			if (comma)
				parse_error(parser, "trailing `,' not allowed, ignored");
			parser_next(parser);
			break;
		}

		if (!comma && array_size(decl->init_declrs) > 0)
			parse_error(parser, "comma was expected");
		parse_init_declr(parser, array_push_new(decl->init_declrs));

		comma = false;
		if (token_is(parser->token, TOKEN_COMMA)) {
			comma = true;
			parser_next(parser);
		}
	}
}

/*
 * Parses: 6.7 declaration, 6.7 init-declarator-list.
 * TODO get rid of this allocating wrapper
 */
struct ast_decl *parser_parse_decl(struct parser *parser)
{
	struct ast_decl *decl = mcc_malloc(sizeof(*decl));
	parse_decl_internal(parser, decl);
	return decl;
}

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

void print_decl(struct ast_decl *decl, struct strbuf *buf);

/*
 * NOTE: This function is used by dump_expr.
 */
void print_declspec(struct ast_declspec *dspec, struct strbuf *buf)
{
	size_t i;

	if (dspec->tquals & TQUAL_CONST)
		strbuf_printf(buf, "const ");
	if (dspec->tquals & TQUAL_RESTRICT)
		strbuf_printf(buf, "restrict ");
	if (dspec->tquals & TQUAL_VOLATILE)
		strbuf_printf(buf, "volatile ");

	if (dspec->tflags & TFLAG_UNSIGNED)
		strbuf_printf(buf, "unsigned ");
	else if (dspec->tflags & TFLAG_SIGNED)
		strbuf_printf(buf, "signed ");
	
	if (dspec->tflags & TFLAG_SHORT)
		strbuf_printf(buf, "short ");
	else if (dspec->tflags & TFLAG_LONG)
		strbuf_printf(buf, "long ");
	else if (dspec->tflags & TFLAG_LONG_LONG)
		strbuf_printf(buf, "long long ");
	
	if (dspec->tflags & TFLAG_COMPLEX)
		strbuf_printf(buf, "_Complex ");

	strbuf_printf(buf, "%s ", tspec_to_string(dspec->tspec));

	if (dspec->tspec & TSPEC_STRUCT) {
		strbuf_printf(buf, "%s ",
			dspec->su_spec->name ? dspec->su_spec->name : ANON_STRUCT_NAME);

		if (dspec->su_spec->members) {
			strbuf_printf(buf, "{\n");
			for (i = 0; i < array_size(dspec->su_spec->members); i++) {
				strbuf_printf(buf, "\t");
				print_decl(&dspec->su_spec->members[i], buf);
				strbuf_printf(buf, "\n");
			}
			strbuf_printf(buf, "} ");
		}
	}
	if (dspec->tspec & TSPEC_UNION) {
		TMP_ASSERT(0);
	}
}

static void print_init_declr(struct ast_init_declr *init_decl, struct strbuf *buf)
{
	struct ast_declr *declr;
	bool need_parens = false;
	struct strbuf decl_buf;
	struct strbuf declspec_buf;

	strbuf_init(&decl_buf, 8);
	strbuf_init(&declspec_buf, 16);

	strbuf_printf(&decl_buf, init_decl->ident);
	for (size_t i = 0; i < array_size(init_decl->declrs); i++) {
		declr = &init_decl->declrs[i];
		switch (declr->type) {
			case DECLR_TYPE_ARRAY:
				if (need_parens) {
					strbuf_prepend(&decl_buf, "(");
					strbuf_printf(&decl_buf, ")");
				}

				strbuf_printf(&decl_buf, "[");
				if (declr->size)
					dump_expr(declr->size, &decl_buf);
				strbuf_printf(&decl_buf, "]");
				need_parens = false;
				break;
			case DECLR_TYPE_PTR:
				strbuf_reset(&declspec_buf);
				//print_declspec(&declr->declspec, &declspec_buf);
				if (strbuf_strlen(&declspec_buf) > 0)
					strbuf_prepend(&decl_buf, "* %s",
						strbuf_get_string(&declspec_buf));
				else
					strbuf_prepend(&decl_buf, "*");
				need_parens = true;
				break;
			default:
				TMP_ASSERT(false);
		}
	}

	if (init_decl->init) {
		strbuf_printf(&decl_buf, " = ");
		dump_expr(init_decl->init, &decl_buf);
	}

	strbuf_printf(buf, strbuf_get_string(&decl_buf));
	strbuf_free(&decl_buf);
}

void print_decl(struct ast_decl *decl, struct strbuf *buf)
{
	size_t i;

	print_declspec(&decl->declspec, buf);

	for (i = 0; i < array_size(decl->init_declrs); i++) {
		if (i > 0)
			strbuf_printf(buf, ", ");
		print_init_declr(&decl->init_declrs[i], buf);
	}
	strbuf_printf(buf, ";");
}

void dump_decl(struct ast_decl *decl)
{
	struct strbuf buf;
	strbuf_init(&buf, 64);

	print_decl(decl, &buf);

	fprintf(stderr, strbuf_get_string(&buf));
	strbuf_free(&buf);
}
