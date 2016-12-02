/*
 * TODO Assuming that list_node is first member of struct tokinfo.
 *      Define a container_of macro to drop this requirement.
 * TODO Add #pragma and #line support.
 */

#include "common.h"
#include "debug.h"
#include "lexer.h"
#include "symtab.h"
#include <assert.h>
#include <stdarg.h>


/*
 * Artificial item to be kept at the bottom of the if-stack to avoid special
 * cases in the code. Its presence is equivalent to the whole cppfile being
 * wrapped in a big #if 1...#endif block.
 */
static struct cpp_if ifstack_bottom = {
	.skip_this_branch = false,
	.skip_next_branch = true /* no other branches */
};


static const struct {
	char *name;
	enum cpp_directive directive;
} directives[] = {
	{ .name = "if", .directive = CPP_DIRECTIVE_IF },
	{ .name = "ifdef", .directive = CPP_DIRECTIVE_IFDEF },
	{ .name = "ifndef", .directive = CPP_DIRECTIVE_IFNDEF },
	{ .name = "elif", .directive = CPP_DIRECTIVE_ELIF },
	{ .name = "else", .directive = CPP_DIRECTIVE_ELSE },
	{ .name = "endif", .directive = CPP_DIRECTIVE_ENDIF },
	{ .name = "include", .directive = CPP_DIRECTIVE_INCLUDE },
	{ .name = "define", .directive = CPP_DIRECTIVE_DEFINE },
	{ .name = "undef", .directive = CPP_DIRECTIVE_UNDEF },
	{ .name = "line", .directive = CPP_DIRECTIVE_LINE },
	{ .name = "error", .directive = CPP_DIRECTIVE_ERROR },
	{ .name = "pragma", .directive = CPP_DIRECTIVE_PRAGMA },
};


static void cpp_setup_symtab_directives(struct cppfile *file)
{
	struct symbol *symbol;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(directives); i++) {
		symbol = symtab_insert(file->symtab, directives[i].name);
		symbol->type = SYMBOL_TYPE_CPP_DIRECTIVE;
		symbol->directive = directives[i].directive;
	}
}


static void cpp_setup_symtab_builtins(struct cppfile *file)
{
	struct symbol *symbol;
	size_t i;

	char *builtins[] = {
		"__""LINE__", // TODO better way to escape this?
		"__""FILE__",
		"__""TIME__"
	};

	for (i = 0; i < ARRAY_SIZE(builtins); i++) {
		symbol = symtab_insert(file->symtab, builtins[i]);
		symbol->type = SYMBOL_TYPE_CPP_BUILTIN;
	}
}


void cpp_setup_symtab(struct cppfile *file)
{
	cpp_setup_symtab_directives(file);
	cpp_setup_symtab_builtins(file);
}


static struct tokinfo *cpp_pop(struct cppfile *file)
{
	struct tokinfo *tokinfo;
	struct tokinfo *tmp;

	if (list_is_empty(&file->tokens)) {
		tokinfo = lexer_next(file);
		if (tokinfo)
			list_insert_last(&file->tokens, &tokinfo->list_node);
	}

	assert(list_length(&file->tokens) > 0); /* at least TOKEN_EOF/TOKEN_EOL */

	tmp = file->cur;
	file->cur = list_remove_first(&file->tokens);
	return tmp;
}


static bool cpp_got(struct cppfile *file, enum token token)
{
	return file->cur->token == token;
}


static inline bool cpp_got_eol(struct cppfile *file)
{
	return cpp_got(file, TOKEN_EOL);
}


static inline bool cpp_got_eof(struct cppfile *file)
{
	return cpp_got(file, TOKEN_EOF);
}


static inline bool cpp_got_eol_eof(struct cppfile *file)
{
	return cpp_got_eol(file) || cpp_got_eof(file);
}


static bool cpp_got_hash(struct cppfile *file)
{
	return cpp_got(file, TOKEN_HASH) && file->cur->is_at_bol;
}


static bool cpp_directive(struct cppfile *file, enum cpp_directive directive)
{
	return cpp_got(file, TOKEN_NAME)
		&& file->cur->symbol->type == SYMBOL_TYPE_CPP_DIRECTIVE
		&& file->cur->symbol->directive == directive;
}


static void cpp_skip_line(struct cppfile *file)
{
	while (!cpp_got_eof(file)) {
		if (cpp_got(file, TOKEN_EOL)) {
			cpp_pop(file);
			break;
		}
		cpp_pop(file);
	}
}


static inline void cpp_skip_rest_of_directive(struct cppfile *file)
{
	while (!cpp_got_eof(file) && !cpp_got_eol(file))
		cpp_pop(file);
}


static bool cpp_expect(struct cppfile *file, enum token token)
{
	if (file->cur->token == token)
		return true;

	cppfile_error(file, "%s was expected, got %s",
		token_get_name(token), token_get_name(file->cur->token));

	return false;
}


static void cpp_match_eol_eof(struct cppfile *file)
{
	if (!cpp_got(file, TOKEN_EOL) && !cpp_got_eof(file)) {
		cppfile_error(file, "extra tokens will be skipped");
		cpp_skip_line(file);
	}
	else if (cpp_got(file, TOKEN_EOL)) {
		cpp_pop(file);
	}
}


static bool cpp_expect_directive(struct cppfile *file)
{
	if (!cpp_expect(file, TOKEN_NAME))
		return false;

	if (file->cur->symbol->type != SYMBOL_TYPE_CPP_DIRECTIVE) {
		cppfile_error(file, "'%s' is not a C preprocessor directive",
			symbol_get_name(file->cur->symbol));

		return false;
	}

	return true;
}


static struct cpp_if *cpp_ifstack_push(struct cppfile *file, struct tokinfo *tokinfo)
{
	struct cpp_if *cpp_if = mempool_alloc(&file->token_data, sizeof(*cpp_if));
	cpp_if->tokinfo = tokinfo;
	cpp_if->skip_this_branch = false;
	cpp_if->skip_next_branch = false;

	list_insert_first(&file->ifs, &cpp_if->list_node);

	return cpp_if;
}


static struct cpp_if *cpp_ifstack_pop(struct cppfile *file)
{
	return list_remove_first(&file->ifs);
}


static struct cpp_if *cpp_ifstack_top(struct cppfile *file)
{
	return list_first(&file->ifs);
}


static inline bool cpp_skipping(struct cppfile *file)
{
	return cpp_ifstack_top(file)->skip_this_branch;
}


static void dump_macro(struct cpp_macro *macro)
{
	struct tokinfo *arg;
	struct tokinfo *repl;
	struct strbuf buf;

	strbuf_init(&buf, 128);
	strbuf_printf(&buf, "%s", macro->name);

	if (macro->type == CPP_MACRO_TYPE_FUNCLIKE) {
		strbuf_putc(&buf, '(');
		for (arg = list_first(&macro->arglist); arg; arg = list_next(&arg->list_node)) {
			tokinfo_print(arg, &buf);
			if (arg != list_last(&macro->arglist))
				strbuf_printf(&buf, ", ");
		}
		strbuf_putc(&buf, ')');
	}

	strbuf_putc(&buf, '\t');

	for (repl = list_first(&macro->repl_list); repl; repl = list_next(&repl->list_node)) {
		tokinfo_print(repl, &buf);
		strbuf_putc(&buf, ' ');
	}

	printf("%s\n", strbuf_get_string(&buf));
	strbuf_free(&buf);
}


static void cpp_parse_macro_arglist(struct cppfile *file, struct cpp_macro *macro)
{
	bool expect_comma = false;
	bool arglist_ended = false;

	while (!cpp_got_eol_eof(file)) {
		if (cpp_got(file, TOKEN_LPAREN)) {
			cppfile_error(file, "the '(' may not appear inside macro argument list");
			break;
		}

		if (cpp_got(file, TOKEN_COMMA)) {
			if (expect_comma)
				expect_comma = false;
			else
				cppfile_error(file, "comma was unexpected here");

			cpp_pop(file);
			continue;
		}

		if (cpp_got(file, TOKEN_RPAREN)) {
			arglist_ended = true;
			cpp_pop(file);
			break;
		}

		if (expect_comma)
			cppfile_error(file, "comma was expected here");

		list_insert_last(&macro->arglist, &file->cur->list_node);
		expect_comma = true;
		cpp_pop(file);
	}

	if (!arglist_ended)
		cppfile_error(file, "macro arglist misses terminating parentheses");
}


static void cpp_parse_define(struct cppfile *file)
{
	struct tokinfo *tmp;

	if (!cpp_expect(file, TOKEN_NAME)) {
		cpp_skip_line(file);
		return;
	}

	if (!cpp_skipping(file))
		file->cur->symbol->type = SYMBOL_TYPE_CPP_MACRO;

	struct cpp_macro *macro = objpool_alloc(&file->macro_pool);
	macro->name = symbol_get_name(file->cur->symbol);
	list_init(&macro->arglist);
	list_init(&macro->repl_list);

	cpp_pop(file);

	if (file->cur->token == TOKEN_LPAREN) {
		DEBUG_EXPR("%i", file->cur->preceded_by_whitespace);
		if (!file->cur->preceded_by_whitespace) {
			cpp_pop(file);
			cpp_parse_macro_arglist(file, macro);
			macro->type = CPP_MACRO_TYPE_FUNCLIKE;
		}
	}
	else {
		macro->type = CPP_MACRO_TYPE_VARLIKE;
	}

	while (!cpp_got_eol_eof(file)) {
		tmp = cpp_pop(file);
		list_insert_last(&macro->repl_list, &tmp->list_node);
	}

	dump_macro(macro);
}


static void cpp_parse_undef(struct cppfile *file)
{
	if (!cpp_expect(file, TOKEN_NAME)) {
		cpp_skip_line(file);
		return;
	}

	if (!cpp_skipping(file))
		file->cur->symbol->type = SYMBOL_TYPE_UNDEF;

	cpp_pop(file);
}


static void cpp_parse_error(struct cppfile *file)
{
	cppfile_error(file, "%s", file->cur->str);
	cpp_pop(file);
	cpp_match_eol_eof(file);
}


/*
 * TODO #include <header> vs #include "header".
 */
static void cpp_parse_include(struct cppfile *file)
{
	char *filename;
	struct cppfile *included_file;

	if (cpp_expect(file, TOKEN_HEADER_NAME))
		DEBUG_PRINTF("#include header file '%s'", file->cur->str);

	if (!cpp_skipping(file)) {
		filename = file->cur->str;
		included_file = cppfile_new();
		if (!included_file)
			return;

		if (cppfile_open(included_file, filename) != MCC_ERROR_OK) {
			cppfile_error(file, "cannot include file %s: access denied",
				filename);
			cppfile_delete(included_file);
		}
		else {
			file->included_file = included_file;
			cppfile_set_symtab(file->included_file, file->symtab);
		}
	}

	cpp_pop(file);
	file->inside_include = false;
}


static void cpp_parse_directive(struct cppfile *file)
{
	struct cpp_if *cpp_if;
	enum cpp_directive dir;
	bool skipping;
	bool test_cond;

	if (!cpp_expect_directive(file)) {
		cpp_skip_line(file);
		return;
	}

	dir = file->cur->symbol->directive;

	file->inside_include = cpp_directive(file, CPP_DIRECTIVE_INCLUDE);
	cpp_pop(file);

	switch (dir) {
	case CPP_DIRECTIVE_IFDEF:
		test_cond = file->cur->symbol->type == SYMBOL_TYPE_CPP_MACRO;
		cpp_pop(file);
		goto push_if;

	case CPP_DIRECTIVE_IFNDEF:
		test_cond = file->cur->symbol->type != SYMBOL_TYPE_CPP_MACRO;
		cpp_pop(file);
		goto push_if;

	case CPP_DIRECTIVE_IF:
		test_cond = true; /* TODO */

push_if:
		skipping = cpp_skipping(file);
		cpp_if = cpp_ifstack_push(file, file->cur);
		cpp_if->skip_next_branch = skipping;

		goto conclude;

	case CPP_DIRECTIVE_ELIF:
		test_cond = true; /* TODO */
		cpp_if = cpp_ifstack_top(file);

conclude:
		cpp_if->skip_this_branch = !test_cond || cpp_if->skip_next_branch;
		cpp_if->skip_next_branch |= !cpp_if->skip_this_branch;

		break;
	
	case CPP_DIRECTIVE_ELSE:
		/* TODO check elif after else */
		cpp_if = cpp_ifstack_top(file);
		cpp_if->skip_this_branch = cpp_if->skip_next_branch;
		break;

	case CPP_DIRECTIVE_ENDIF:
		assert(cpp_ifstack_top(file) != &ifstack_bottom); /* TODO */

		if (cpp_ifstack_top(file) == &ifstack_bottom)
			cppfile_error(file, "endif without matching if/ifdef/ifndef");
		else
			cpp_ifstack_pop(file);

		break;

	case CPP_DIRECTIVE_DEFINE:
		cpp_parse_define(file);
		break;

	case CPP_DIRECTIVE_INCLUDE:
		cpp_parse_include(file);

		/*
		 * TODO To keep tokens after the #include "filename" (if any).
		 *      How to get rid of this?
		 */
		return;

	case CPP_DIRECTIVE_ERROR:
		cpp_parse_error(file);
		break;

	case CPP_DIRECTIVE_UNDEF:
		cpp_parse_undef(file);
		break;

	default:
		return;
	}

	cpp_match_eol_eof(file);
}


static void cpp_parse(struct cppfile *file)
{
	if (file->cur == NULL) { /* TODO Get rid of this special case */
		cpp_pop(file); 
		list_insert_first(&file->ifs, &ifstack_bottom.list_node);
	}

	while (!cpp_got_eof(file)) {
		if (!cpp_got_hash(file)) {
			if (!cpp_skipping(file))
				break;

			cpp_pop(file);
		}
		else {
			cpp_pop(file);
			cpp_parse_directive(file);
		}
	}
}


struct tokinfo *cpp_next(struct cppfile *file)
{
	struct tokinfo *next;

included:
	if (file->included_file) {
		next = cpp_next(file->included_file);

		if (next->token == TOKEN_EOF) {
			cppfile_close(file->included_file);
			cppfile_delete(file->included_file);
			file->included_file = NULL;
		}
		else {
			return next;
		}
	}

	cpp_parse(file);
	if (file->included_file)
		goto included;

	return cpp_pop(file);
}
