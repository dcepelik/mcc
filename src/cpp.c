#include "common.h"
#include "debug.h"
#include "lexer.h"
#include "symtab.h"
#include <assert.h>
#include <stdarg.h>


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


static void cpp_skip_ifbranch(struct cppfile *file);


static void cpp_skip_if(struct cppfile *file)
{
	DEBUG_TRACE;

	assert(cpp_directive(file, CPP_DIRECTIVE_IF)
		|| cpp_directive(file, CPP_DIRECTIVE_IFDEF)
		|| cpp_directive(file, CPP_DIRECTIVE_IFNDEF)
		|| cpp_directive(file, CPP_DIRECTIVE_ELIF)
		|| cpp_directive(file, CPP_DIRECTIVE_ELSE));

skip_another_branch:
	if (!cpp_directive(file, CPP_DIRECTIVE_ELSE) && !cpp_directive(file, CPP_DIRECTIVE_ENDIF))
		cpp_skip_rest_of_directive(file); /* ignore the condition */
	else
		cpp_pop(file);

	cpp_match_eol_eof(file);

	cpp_skip_ifbranch(file);

	if (cpp_got_eof(file)) {
		cppfile_error(file, "unexpected EOF, endif was expected");
		return;
	}

	if (cpp_directive(file, CPP_DIRECTIVE_ENDIF)) {
		cpp_pop(file);
		cpp_match_eol_eof(file);
		return;
	}

	goto skip_another_branch;
}


static void cpp_skip_directive(struct cppfile *file)
{
	if (!cpp_expect_directive(file))
		cpp_skip_line(file);

	switch (file->cur->symbol->directive) {
	case CPP_DIRECTIVE_IF:
	case CPP_DIRECTIVE_IFDEF:
	case CPP_DIRECTIVE_IFNDEF:
		cpp_skip_if(file);
		break;

	default:
		cpp_skip_line(file);
	}
}


static void cpp_skip_ifbranch(struct cppfile *file)
{
	DEBUG_TRACE;

	while (!cpp_got_eof(file)) {
		if (!cpp_got_hash(file)) {
			cpp_skip_line(file);
			continue;
		}

		cpp_pop(file);

		if (!cpp_expect_directive(file)) {
			cpp_skip_line(file);
			return;
		}

		switch (file->cur->symbol->directive) {
		case CPP_DIRECTIVE_ELIF:
		case CPP_DIRECTIVE_ELSE:
		case CPP_DIRECTIVE_ENDIF:
		case CPP_DIRECTIVE_IF:
			return;

		default:
			cpp_skip_directive(file);
		}
	}
}


static void cpp_parse(struct cppfile *file);


static void cpp_parse_ifbranch(struct cppfile *file)
{
	DEBUG_TRACE;
	cpp_parse(file);
}


static void cpp_parse_if(struct cppfile *file)
{
	DEBUG_TRACE;

	assert(cpp_got_eof(file)
		|| cpp_directive(file, CPP_DIRECTIVE_IF)
		|| cpp_directive(file, CPP_DIRECTIVE_ELIF)
		|| cpp_directive(file, CPP_DIRECTIVE_ELSE)
		|| cpp_directive(file, CPP_DIRECTIVE_ENDIF));

	while (!cpp_got_eof(file)) {
		if (cpp_directive(file, CPP_DIRECTIVE_ENDIF)) {
			cpp_pop(file);
			cpp_skip_line(file);
			return;
		}

		if (cpp_directive(file, CPP_DIRECTIVE_ELSE)) {
			cpp_pop(file);
			cpp_match_eol_eof(file);
			cpp_parse_ifbranch(file);
			return;
		}

		cpp_skip_ifbranch(file);
	}

	cppfile_error(file, "EOF was unexpected, expected endif");
}


static void cpp_parse_ifdef(struct cppfile *file, bool ifndef)
{
	DEBUG_TRACE;

	if (!cpp_expect(file, TOKEN_NAME)) {
		cpp_warn("skipping rest of the if directive");
		cpp_skip_if(file);
	}
	else if ((file->cur->symbol->type == SYMBOL_TYPE_CPP_MACRO) == !ifndef) {
		cpp_pop(file);
		cpp_match_eol_eof(file);
		cpp_parse_ifbranch(file);
	}
	else {
		cpp_pop(file);
		cpp_match_eol_eof(file);
		cpp_skip_ifbranch(file);
		cpp_parse_if(file);
	}
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
	if (!cpp_expect_directive(file)) {
		cpp_skip_line(file);
		return;
	}

	/*
	 * TODO Right now, I am assuming correct #if..#elif..#else..#endif
	 *      constructs. Add error-handling.
	 */

	switch (file->cur->symbol->directive) {
	case CPP_DIRECTIVE_ERROR:
		cpp_pop(file);
		cpp_parse_error(file);
		break;

	case CPP_DIRECTIVE_INCLUDE:
		file->inside_include = true;
		cpp_pop(file);
		cpp_parse_include(file);
		break;

	case CPP_DIRECTIVE_IFDEF:
		cpp_pop(file);
		cpp_parse_ifdef(file, false);
		break;

	case CPP_DIRECTIVE_IFNDEF:
		cpp_pop(file);
		cpp_parse_ifdef(file, true);
		break;

	case CPP_DIRECTIVE_DEFINE:
		cpp_pop(file);
		cpp_parse_define(file);
		break;

	case CPP_DIRECTIVE_ELIF:
	case CPP_DIRECTIVE_ELSE:
		/*
		 * OK, so we must have been parsing an if-branch. If that
		 * branch was activated, others are to be skipped. 
		 *
		 * TODO Validation.
		 */
		cpp_skip_if(file);
		break;

	case CPP_DIRECTIVE_ENDIF:
		/*
		 * Same as above.
		 *
		 * TODO Validation.
		 */
		cpp_pop(file);
		cpp_match_eol_eof(file);
		break;

	default:
		cppfile_error(file, "directive %s not supported", NULL);
		cpp_skip_line(file);
	}
}


static void cpp_parse(struct cppfile *file)
{
	if (file->cur == NULL)
		cpp_pop(file); /* TODO Get rid of this special case */

	while (cpp_got_hash(file)) {
		cpp_pop(file);
		cpp_parse_directive(file);
	}
}


struct tokinfo *cpp_next(struct cppfile *file)
{
	cpp_parse(file);
	return cpp_pop(file);

}
