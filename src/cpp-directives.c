#include "context.h"
#include "cpp-internal.h"

#define VA_ARGS_NAME	"__VA_ARGS__"


/*
 * Artificial item to be kept at the bottom of the if stack to avoid special
 * cases in the code. Its presence is equivalent to the whole file being
 * wrapped in a big #if 1...#endif block.
 */
static struct cpp_if ifstack_bottom = {
	.token = NULL,
	.skip_this_branch = false,
	.skip_next_branch = true /* no other branches */
};


/*
 * <include> search paths.
 */
static const char *include_dirs[] = {
	".",
	"",
	"/usr/include",
	"../include"
};


void cpp_init_ifstack(struct cpp *cpp)
{
	list_init(&cpp->ifs);
	list_insert_first(&cpp->ifs, &ifstack_bottom.list_node);
}


static inline bool cpp_directive(struct cpp *cpp, enum cpp_directive directive)
{
	return token_is(cpp->token, TOKEN_NAME)
		&& cpp->token->symbol->def->type == SYMBOL_TYPE_CPP_DIRECTIVE
		&& cpp->token->symbol->def->directive == directive;
}


static inline void cpp_skip_till_eol(struct cpp *cpp)
{
	/* TODO Consider freeing those tokens */

	assert(cpp_cur_file(cpp)->lexer.emit_eols == true);

	while (!token_is_eol_or_eof(cpp->token))
		cpp_next_token(cpp);

	if (token_is_eol(cpp->token))
		cpp_next_token(cpp);
}


static inline void cpp_skip_find_eol(struct cpp *cpp)
{
	/* TODO Consider freeing those tokens */

	assert(cpp_cur_file(cpp)->lexer.emit_eols == true);

	while (!token_is_eol_or_eof(cpp->token))
		cpp_next_token(cpp);
}


static inline bool cpp_expect(struct cpp *cpp, enum token_type token)
{
	if (cpp->token->type == token)
		return true;

	cpp_error(cpp, "%s was expected, got %s",
		token_get_name(token), token_get_name(cpp->token->type));

	return false;
}


static inline bool cpp_expect_directive(struct cpp *cpp)
{
	if (!cpp_expect(cpp, TOKEN_NAME))
		return false;

	if (cpp->token->symbol->def->type != SYMBOL_TYPE_CPP_DIRECTIVE) {
		cpp_error(cpp, "'%s' is not a C preprocessor directive",
			symbol_get_name(cpp->token->symbol));

		return false;
	}

	return true;
}

static inline void cpp_match_eol_or_eof(struct cpp *cpp)
{
	if (!token_is(cpp->token, TOKEN_EOL) && !token_is_eof(cpp->token)) {
		cpp_warn(cpp, "extra tokens will be skipped");
		cpp_skip_till_eol(cpp);
	}
	else if (token_is(cpp->token, TOKEN_EOL)) {
		cpp_next_token(cpp);
	}
}


static struct cpp_if *cpp_ifstack_push(struct cpp *cpp, struct token *token)
{
	/* TODO Use a different pool for struct cpp_if? */
	struct cpp_if *cpp_if = mempool_alloc(&cpp->ctx->token_data, sizeof(*cpp_if));
	cpp_if->token = token;
	cpp_if->skip_this_branch = false;
	cpp_if->skip_next_branch = false;

	list_insert_first(&cpp->ifs, &cpp_if->list_node);

	return cpp_if;
}


static struct cpp_if *cpp_ifstack_pop(struct cpp *cpp)
{
	return list_remove_first(&cpp->ifs);
}


static struct cpp_if *cpp_ifstack_top(struct cpp *cpp)
{
	return list_first(&cpp->ifs);
}


bool cpp_skipping(struct cpp *cpp)
{
	return cpp_ifstack_top(cpp)->skip_this_branch;
}


static void cpp_parse_macro_arglist(struct cpp *cpp, struct macro *macro)
{
	bool expect_comma = false;
	bool arglist_ended = false;

	while (!token_is_eol_or_eof(cpp->token)) {
		if (token_is(cpp->token, TOKEN_LPAREN)) {
			cpp_error(cpp, "the '(' may not appear inside macro argument list");
			break;
		}

		if (token_is(cpp->token, TOKEN_COMMA)) {
			if (expect_comma)
				expect_comma = false;
			else
				cpp_error(cpp, "comma was unexpected here");

			cpp_next_token(cpp);
			continue;
		}

		if (token_is(cpp->token, TOKEN_RPAREN)) {
			arglist_ended = true;
			cpp_next_token(cpp);
			break;
		}

		if (expect_comma)
			cpp_error(cpp, "comma was expected here");

		if (cpp->token->type == TOKEN_ELLIPSIS)
			cpp->token->symbol = symtab_search_or_insert(&cpp->ctx->symtab, VA_ARGS_NAME);

		toklist_insert_last(&macro->args, cpp->token);
		expect_comma = true;
		cpp_next_token(cpp);
	}

	if (!arglist_ended)
		cpp_error(cpp, "macro arglist misses terminating )");
}


static void cpp_parse_define(struct cpp *cpp)
{
	struct symdef *symdef;

	if (!cpp_expect(cpp, TOKEN_NAME)) {
		cpp_skip_till_eol(cpp);
		return;
	}

	if (!cpp_skipping(cpp)) {
		symdef = symbol_define(&cpp->ctx->symtab, cpp->token->symbol);
		symdef->type = SYMBOL_TYPE_CPP_MACRO;

		macro_init(&symdef->macro);
		symdef->macro.name = symbol_get_name(cpp->token->symbol);

		cpp_next_token(cpp);

		if (cpp->token->type == TOKEN_LPAREN && !cpp->token->after_white) {
			cpp_next_token(cpp);
			cpp_parse_macro_arglist(cpp, &symdef->macro);
			symdef->macro.flags = MACRO_FLAGS_FUNCLIKE;
			//macro_dump(macro);
		}
		else {
			symdef->macro.flags = MACRO_FLAGS_OBJLIKE;
			//macro_dump(macro);
		}

		while (!token_is_eol_or_eof(cpp->token)) {
			toklist_insert_last(&symdef->macro.expansion, cpp->token);
			cpp_next_token(cpp);
		}
	}
	else {
		cpp_next_token(cpp);
	}

	//symtab_dump(&cpp->ctx->symtab, stderr);
}


static void cpp_parse_undef(struct cpp *cpp)
{
	if (!cpp_expect(cpp, TOKEN_NAME)) {
		cpp_skip_till_eol(cpp);
		return;
	}

	if (!cpp_skipping(cpp)) {
		/* TODO check it's defined */
		//TODO symbol_pop_definition(&cpp->ctx->symtab, cpp->token->symbol); 
	}

	cpp_next_token(cpp);
}


static void cpp_parse_error(struct cpp *cpp)
{
	/* TODO cat tokens to get error message */
	cpp_error(cpp, "%s", "#error");
	cpp_skip_find_eol(cpp);
	//cpp_match_eol_or_eof(cpp);
}


/*
 * TODO #include <header> vs #include "header".
 * TODO warn if tokens skipped
 */
static void cpp_parse_include(struct cpp *cpp)
{
	char *filename;
	struct strbuf pathbuf;
	char *path;
	struct cpp_file *file;
	bool file_open = false;
	size_t i;

	if (cpp_skipping(cpp)) {
		cpp_skip_find_eol(cpp);
		goto out;
	}

	if (cpp->token->type != TOKEN_HEADER_HNAME
		&& cpp->token->type != TOKEN_HEADER_QNAME) {
		cpp_error(cpp, "header name was expected, got %s",
			token_get_name(cpp->token->type));

		cpp_skip_find_eol(cpp);
		goto out;
	}

	/*
	 * TODO Disallow presence of NULs in header names.
	 */

	filename = cpp->token->str;
	strbuf_init(&pathbuf, 128);
	file = objpool_alloc(&cpp->file_pool);

	for (i = 0; i < ARRAY_SIZE(include_dirs); i++) {
		strbuf_reset(&pathbuf);
		strbuf_printf(&pathbuf, "%s/%s", include_dirs[i], filename);

		path = strbuf_get_string(&pathbuf);

		if (cpp_file_init(cpp, file, path) == MCC_ERROR_OK) {
			file_open = true;
			break;
		}
	}

	strbuf_free(&pathbuf);

	if (file_open) {
		cpp_skip_find_eol(cpp); /* get rid of those tokens now */
		cpp_file_include(cpp, file);
	}
	else {
		cpp_error(cpp, "cannot include file %s", filename);
		cpp_skip_find_eol(cpp); /* skip is delayed: error location */
	}

out:
	cpp_cur_file(cpp)->lexer.inside_include = false;
}


void cpp_parse_directive(struct cpp *cpp)
{
	struct cpp_if *cpp_if;
	struct token *prev;
	enum cpp_directive dir;
	bool skipping;
	bool test_cond;

	if (token_is(cpp->token, TOKEN_EOL))
		return; /* null directive */

	if (!cpp_expect_directive(cpp)) {
		cpp_skip_till_eol(cpp);
		return;
	}

	prev = cpp->token;
	dir = cpp->token->symbol->def->directive;

	cpp_cur_file(cpp)->lexer.inside_include
		= cpp_directive(cpp, CPP_DIRECTIVE_INCLUDE);

	cpp_next_token(cpp);

	switch (dir) {
	case CPP_DIRECTIVE_IFDEF:
		test_cond = cpp->token->symbol->def->type == SYMBOL_TYPE_CPP_MACRO;
		cpp_next_token(cpp);
		goto push_if;

	case CPP_DIRECTIVE_IFNDEF:
		test_cond = cpp->token->symbol->def->type != SYMBOL_TYPE_CPP_MACRO;
		cpp_next_token(cpp);
		goto push_if;

	case CPP_DIRECTIVE_IF:
		test_cond = true; /* TODO */

push_if:
		skipping = cpp_skipping(cpp);
		cpp_if = cpp_ifstack_push(cpp, prev);
		cpp_if->skip_next_branch = skipping;

		goto conclude;

	case CPP_DIRECTIVE_ELIF:
		test_cond = true; /* TODO */
		cpp_if = cpp_ifstack_top(cpp);

conclude:
		cpp_if->skip_this_branch = !test_cond || cpp_if->skip_next_branch;
		cpp_if->skip_next_branch |= !cpp_if->skip_this_branch;
		break;
	
	case CPP_DIRECTIVE_ELSE:
		/* TODO check elif after else */
		cpp_if = cpp_ifstack_top(cpp);
		cpp_if->skip_this_branch = cpp_if->skip_next_branch;
		break;

	case CPP_DIRECTIVE_ENDIF:
		assert(cpp_ifstack_top(cpp) != &ifstack_bottom); /* TODO */

		if (cpp_ifstack_top(cpp) == &ifstack_bottom)
			cpp_error(cpp, "endif without matching if/ifdef/ifndef");
		else
			cpp_ifstack_pop(cpp);

		break;

	case CPP_DIRECTIVE_DEFINE:
		cpp_parse_define(cpp);
		//symtab_dump(&cpp->ctx->symtab, stderr);
		break;

	case CPP_DIRECTIVE_INCLUDE:
		cpp_parse_include(cpp);

		/*
		 * TODO To keep tokens after the #include "filename" (if any).
		 *      How to get rid of this?
		 */
		return;

	case CPP_DIRECTIVE_ERROR:
		cpp_parse_error(cpp);
		break;

	case CPP_DIRECTIVE_UNDEF:
		cpp_parse_undef(cpp);
		break;

	default:
		return;
	}

	cpp_match_eol_or_eof(cpp);
	cpp_cur_file(cpp)->lexer.emit_eols = false;
}
