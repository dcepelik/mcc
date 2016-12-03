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
	struct symdef *symdef;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(directives); i++) {
		symbol = symtab_insert(file->symtab, directives[i].name);

		symdef = objpool_alloc(&file->symdef_pool);
		symdef->type = SYMBOL_TYPE_CPP_DIRECTIVE;
		symdef->directive = directives[i].directive;

		symbol_push_definition(file->symtab, symbol, symdef);
	}
}


static void cpp_setup_symtab_builtins(struct cppfile *file)
{
	struct symbol *symbol;
	struct symdef *symdef;
	size_t i;

	char *builtins[] = {
		"__""LINE__", // TODO better way to escape this?
		"__""FILE__",
		"__""TIME__"
	};

	for (i = 0; i < ARRAY_SIZE(builtins); i++) {
		symbol = symtab_insert(file->symtab, builtins[i]);

		symdef = objpool_alloc(&file->symdef_pool);
		symdef->type = SYMBOL_TYPE_CPP_BUILTIN;

		symbol_push_definition(file->symtab, symbol, symdef);
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

	assert(!list_is_empty(&file->tokens)); /* at least TOKEN_EOF/TOKEN_EOL */

	tmp = file->cur;
	file->cur = list_remove_first(&file->tokens);
	return tmp;
}


static struct tokinfo *cpp_peek(struct cppfile *file)
{
	struct tokinfo *tmp;
	struct tokinfo *peek;

	/* TODO Rewrite this, this ain't nice. */

	tmp = file->cur;
	cpp_pop(file);
	peek = file->cur;

	list_insert_first(&file->tokens, &peek->list_node);
	file->cur = tmp;

	assert(peek != NULL);

	return peek;
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
		&& file->cur->symbol->def->type == SYMBOL_TYPE_CPP_DIRECTIVE
		&& file->cur->symbol->def->directive == directive;
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


static bool cpp_expect_directive(struct cppfile *file)
{
	if (!cpp_expect(file, TOKEN_NAME))
		return false;

	if (file->cur->symbol->def->type != SYMBOL_TYPE_CPP_DIRECTIVE) {
		cppfile_error(file, "'%s' is not a C preprocessor directive",
			symbol_get_name(file->cur->symbol));

		return false;
	}

	return true;
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

	strbuf_printf(&buf, " -> ");

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
		cppfile_error(file, "macro arglist misses terminating )");
}


static void cpp_parse_macro_args(struct cppfile *file, struct cpp_macro *macro)
{
	unsigned parens_balance = 0;
	struct tokinfo *arg;
	struct symdef *argdef;
	bool args_ended = false;
	struct strbuf buf;

	strbuf_init(&buf, 1024);

	assert(macro->type != CPP_MACRO_TYPE_FUNCLIKE || file->cur->token == TOKEN_LPAREN);
	cpp_pop(file); /* ( */

	for (arg = list_first(&macro->arglist); arg; arg = list_next(&arg->list_node)) {
		argdef = objpool_alloc(&file->symdef_pool);
		argdef->type = SYMBOL_TYPE_CPP_MACRO_ARG;
		list_init(&argdef->tokens);

		symbol_push_definition(file->symtab, arg->symbol, argdef);

		DEBUG_PRINTF("Parsing argument '%s'", symbol_get_name(arg->symbol));

		while (!cpp_got_eof(file)) {
			tokinfo_print(file->cur, &buf);
			DEBUG_PRINTF("Argument item: %s", strbuf_get_string(&buf));
			strbuf_reset(&buf);
			if (cpp_got(file, TOKEN_LPAREN)) {
				parens_balance++;
			}
			else if (cpp_got(file, TOKEN_RPAREN)) {
				if (parens_balance == 0) {
					args_ended = true;
					break;
				}
				else {
					parens_balance--;
				}
			}
			else if (cpp_got(file, TOKEN_COMMA)) {
				if (parens_balance == 0)
					break;
			}

			list_insert_last(&argdef->tokens, &cpp_pop(file)->list_node);
		}

		cpp_pop(file);

		if (args_ended)
			break;
	}

	if (!args_ended)
		cppfile_error(file, "too many arguments or unterminated arglist");
}


static void cpp_parse_define(struct cppfile *file)
{
	struct symdef *symdef;
	struct cpp_macro *macro;
	struct tokinfo *tmp;

	if (!cpp_expect(file, TOKEN_NAME)) {
		cpp_skip_line(file);
		return;
	}

	if (cpp_skipping(file))
		cpp_skip_line(file);

	macro = objpool_alloc(&file->macro_pool);
	macro->name = symbol_get_name(file->cur->symbol);
	list_init(&macro->arglist);
	list_init(&macro->repl_list);

	symdef = objpool_alloc(&file->symdef_pool);
	symdef->type = SYMBOL_TYPE_CPP_MACRO;
	symdef->macro = macro;
	symbol_push_definition(file->symtab, file->cur->symbol, symdef);

	cpp_pop(file);

	if (file->cur->token == TOKEN_LPAREN && !file->cur->preceded_by_whitespace) {
		cpp_pop(file);
		cpp_parse_macro_arglist(file, macro);
		macro->type = CPP_MACRO_TYPE_FUNCLIKE;
	}
	else {
		macro->type = CPP_MACRO_TYPE_OBJLIKE;
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

	if (!cpp_skipping(file)) {
		/* TODO check it's defined */
		symbol_pop_definition(file->symtab, file->cur->symbol); 
	}

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

	if (cpp_expect(file, TOKEN_HEADER_NAME) && !cpp_skipping(file)) {
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

	dir = file->cur->symbol->def->directive;

	file->inside_include = cpp_directive(file, CPP_DIRECTIVE_INCLUDE);
	cpp_pop(file);

	switch (dir) {
	case CPP_DIRECTIVE_IFDEF:
		test_cond = file->cur->symbol->def->type == SYMBOL_TYPE_CPP_MACRO;
		cpp_pop(file);
		goto push_if;

	case CPP_DIRECTIVE_IFNDEF:
		test_cond = file->cur->symbol->def->type != SYMBOL_TYPE_CPP_MACRO;
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


static bool cpp_cur_is_expandable(struct cppfile *file)
{
	struct cpp_macro *macro;
	struct tokinfo *peek;

	if (cpp_got(file, TOKEN_NAME)) {
		if (file->cur->symbol->def->type == SYMBOL_TYPE_CPP_MACRO) {
			macro = file->cur->symbol->def->macro;

			if (macro->type == CPP_MACRO_TYPE_FUNCLIKE) {
				peek = cpp_peek(file);
				return peek->token == TOKEN_LPAREN
					&& !peek->preceded_by_whitespace;
			}
			else {
				return true;
			}
		}
		else if (file->cur->symbol->def->type == SYMBOL_TYPE_CPP_MACRO_ARG) {
			return true;
		}
	}

	return false;
}


static void cpp_dump_toklist(struct cppfile *file, struct list *lst)
{
	struct tokinfo *token;
	struct strbuf buf;

	strbuf_init(&buf, 1024);
	strbuf_printf(&buf, "Toklist: ");
	tokinfo_print(file->cur, &buf);
	strbuf_putc(&buf, ' ');

	int c = 0;

	for (token = list_first(lst); token; token = list_next(&token->list_node)) {
		c++;
		tokinfo_print(token, &buf);
		if (token != list_last(lst))
			strbuf_putc(&buf, ' ');

		if (c > 100) {
			fprintf(stderr, "Breaking out\n");
			break;
		}

	}

	fprintf(stderr, "%s\n", strbuf_get_string(&buf));
	strbuf_free(&buf);
}


static void cpp_copy_toklist(struct cppfile *file, struct list *src, struct list *dst)
{
	/* TODO handle alloc errors */
	struct tokinfo *src_token;
	struct tokinfo *dst_token;

	for (src_token = list_first(src); src_token;
		src_token = list_next(&src_token->list_node)) {

		dst_token = objpool_alloc(&file->tokinfo_pool);

		*dst_token = *src_token;
		list_insert_last(dst, &dst_token->list_node);
	}

	assert(list_length(src) == list_length(dst));
}


static void cpp_expand_macro(struct cppfile *file)
{
	struct cpp_macro *macro;
	struct list toklist;

	assert(cpp_cur_is_expandable(file));
	list_init(&toklist);

	if (file->cur->symbol->def->type == SYMBOL_TYPE_CPP_MACRO) {
		DEBUG_PRINTF("Expanding macro '%s'", symbol_get_name(file->cur->symbol));
		macro = file->cur->symbol->def->macro;
		cpp_pop(file);

		if (macro->type == CPP_MACRO_TYPE_FUNCLIKE)
			cpp_parse_macro_args(file, macro);

		cpp_copy_toklist(file, &macro->repl_list, &toklist);

		list_insert_first(&file->tokens, &file->cur->list_node);
		list_prepend(&file->tokens, &toklist);
		cpp_pop(file);
	}
	else if (file->cur->symbol->def->type == SYMBOL_TYPE_CPP_MACRO_ARG) {
		cpp_copy_toklist(file, &file->cur->symbol->def->tokens, &toklist);
		cpp_pop(file);

		list_insert_first(&file->tokens, &file->cur->list_node);
		list_prepend(&file->tokens, &toklist);
		cpp_pop(file);
	}

	cpp_dump_toklist(file, &file->tokens);
}


static void cpp_parse(struct cppfile *file)
{
	if (file->cur == NULL) { /* TODO Get rid of this special case */
		cpp_pop(file); 
		list_insert_first(&file->ifs, &ifstack_bottom.list_node);
	}

	while (!cpp_got_eof(file)) {
		if (cpp_got_hash(file)) {
			cpp_pop(file);
			cpp_parse_directive(file);

		}
		else if (cpp_cur_is_expandable(file)) {
			/* no cpp_pop(file) here */
			cpp_expand_macro(file);
		}
		else {
			if (!cpp_skipping(file))
				break;

			cpp_pop(file);
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
