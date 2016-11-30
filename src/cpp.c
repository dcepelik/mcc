#include "common.h"
#include "debug.h"
#include "lexer.h"
#include "symtab.h"
#include <assert.h>
#include <stdarg.h>


/*
 * Artificial item at the bottom of the if-stack to avoid special cases
 * in the code. Imagine the whole cppfile is wrapped between #if 1 and #endif.
 */
static struct cpp_if ifstack_bottom = {
	.skip_this_branch = true,
	.skip_other_branches = true /* no other branches */
};


/*
 * TODO Assuming that list_node is first member of struct tokinfo.
 *      Define a container_of macro to drop this requirement.
 */


static const struct {
	const char *name;
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

	const char *builtins[] = {
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


static void cpp_warn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	printf("warning: ");
	vprintf(fmt, args);
	putchar('\n');
	va_end(args);
}


static bool cpp_got(struct cppfile *file, enum token token)
{
	return file->cur->token == token;
}


static bool cpp_got_eol(struct cppfile *file)
{
	return cpp_got(file, TOKEN_EOL);
}


static bool cpp_got_eof(struct cppfile *file)
{
	return cpp_got(file, TOKEN_EOF);
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
		cpp_warn("extra tokens will be skipped");
		cpp_skip_line(file);
	}
	else if (cpp_got(file, TOKEN_EOL)) {
		cpp_pop(file);
	}
}


static void cpp_parse_error(struct cppfile *file)
{
	cppfile_error(file, "%s", file->cur->str);
	cpp_pop(file);
	cpp_match_eol_eof(file);
}


static void cpp_parse_include(struct cppfile *file)
{
	if (cpp_expect(file, TOKEN_HEADER_NAME)) {
		DEBUG_PRINTF("#include header file '%s'", file->cur->str);
	}

	file->inside_include = false;
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
	cpp_if->skip_other_branches = false;

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


static void cpp_parse_define(struct cppfile *file)
{
	DEBUG_TRACE;

	if (!cpp_expect(file, TOKEN_NAME)) {
		cpp_skip_line(file);
		return;
	}

	file->cur->symbol->type = SYMBOL_TYPE_CPP_MACRO;

	cpp_pop(file);
	cpp_match_eol_eof(file);
}


static void cpp_parse_directive(struct cppfile *file)
{
	struct cpp_if *cpp_if;
	struct cpp_if *orig_top;
	bool test_result = true;

	if (!cpp_expect_directive(file)) {
		cpp_skip_line(file);
		return;
	}

	switch (file->cur->symbol->directive) {
	case CPP_DIRECTIVE_IFDEF:
		cpp_pop(file);
		orig_top = cpp_ifstack_top(file);
		cpp_if = cpp_ifstack_push(file, file->cur);
		cpp_if->skip_other_branches = orig_top->skip_this_branch;
		cpp_if->skip_this_branch = !(file->cur->symbol->type == SYMBOL_TYPE_CPP_MACRO);
		file->skip = cpp_if->skip_this_branch;
		cpp_pop(file);
		break;

	case CPP_DIRECTIVE_ELIF:
		//if (cpp_ifstack_empty(file)) {
		//	cppfile_error(file, "elif without preceding if/ifdef/ifndef");
		//	assert(false);
		//}

		cpp_pop(file);
		cpp_if = cpp_ifstack_top(file);
		cpp_if->skip_this_branch = !test_result && !cpp_if->skip_other_branches;
		cpp_if->skip_other_branches |= !cpp_if->skip_this_branch;
		file->skip = cpp_if->skip_this_branch;
		break;
	
	case CPP_DIRECTIVE_ELSE:
		/* TODO check elif after else */
		file->skip = cpp_if->skip_this_branch;
		cpp_pop(file);
		break;

	case CPP_DIRECTIVE_ENDIF:
		assert(cpp_ifstack_top(file) != &ifstack_bottom); /* TODO */

		//if (!cpp_ifstack_empty(file))
			cpp_ifstack_pop(file);
		//else
		//	cppfile_error(file, "endif without matching if/ifdef/ifndef");
		file->skip = cpp_ifstack_top(file)->skip_this_branch;
		cpp_pop(file);
		break;

	case CPP_DIRECTIVE_DEFINE:
		cpp_pop(file);
		cpp_parse_define(file);
		break;
	}
}


static void cpp_parse(struct cppfile *file)
{
	if (file->cur == NULL) { /* TODO Get rid of this special case */
		cpp_pop(file); 
		list_insert_first(&file->ifs, &ifstack_bottom.list_node);
	}

	while (!cpp_got_eof(file)) {
		if (!cpp_got_hash(file)) {
			if (!file->skip)
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
	cpp_parse(file);
	return cpp_pop(file);

}
