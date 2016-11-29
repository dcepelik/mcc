#include "array.h"
#include "common.h"
#include "debug.h"
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


static void cpp_setup_symtab_directives(struct cpp *cpp)
{
	struct symbol *symbol;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(directives); i++) {
		symbol = symtab_insert(cpp->table, directives[i].name);
		symbol->type = SYMBOL_TYPE_CPP_DIRECTIVE;
		symbol->directive = directives[i].directive;
	}
}


static void cpp_setup_symtab_builtins(struct cpp *cpp)
{
	struct symbol *symbol;
	size_t i;

	const char *builtins[] = {
		"__""LINE__", // TODO better way to escape this?
		"__""FILE__",
		"__""TIME__"
	};

	for (i = 0; i < ARRAY_SIZE(builtins); i++) {
		symbol = symtab_insert(cpp->table, builtins[i]);
		symbol->type = SYMBOL_TYPE_CPP_BUILTIN;
	}
}


static void cpp_setup_symtab(struct cpp *cpp)
{
	cpp_setup_symtab_directives(cpp);
	cpp_setup_symtab_builtins(cpp);
}





static struct tokinfo *cpp_pop(struct cpp *cpp)
{
	struct tokinfo *tokinfo;
	struct tokinfo *tmp;

	if (list_is_empty(&cpp->tokens)) {
		tokinfo = lexer_next(&cpp->lexer);
		if (tokinfo)
			list_insert_last(&cpp->tokens, &tokinfo->list_node);
	}

	assert(list_length(&cpp->tokens) > 0); /* at least TOKEN_EOF/TOKEN_EOL */

	tmp = cpp->cur;
	cpp->cur = list_remove_first(&cpp->tokens);
	return tmp;
}


static void cpp_error(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	printf("error: ");
	vprintf(fmt, args);
	putchar('\n');
	va_end(args);
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


static bool cpp_got(struct cpp *cpp, enum token token)
{
	return cpp->cur->token == token;
}


static bool cpp_got_eol(struct cpp *cpp)
{
	return cpp_got(cpp, TOKEN_EOL);
}


static bool cpp_got_eof(struct cpp *cpp)
{
	return cpp_got(cpp, TOKEN_EOF);
}


static bool cpp_got_hash(struct cpp *cpp)
{
	return cpp_got(cpp, TOKEN_HASH) && cpp->cur->is_at_bol;
}


static bool cpp_directive(struct cpp *cpp, enum cpp_directive directive)
{
	return cpp_got(cpp, TOKEN_NAME)
		&& cpp->cur->symbol->type == SYMBOL_TYPE_CPP_DIRECTIVE
		&& cpp->cur->symbol->directive == directive;
}


static void cpp_skip_line(struct cpp *cpp)
{
	while (!cpp_got_eof(cpp)) {
		if (cpp_got(cpp, TOKEN_EOL)) {
			cpp_pop(cpp);
			break;
		}
		cpp_pop(cpp);
	}
}


static inline void cpp_skip_rest_of_directive(struct cpp *cpp)
{
	while (!cpp_got_eof(cpp) && !cpp_got_eol(cpp))
		cpp_pop(cpp);
}


static bool cpp_expect(struct cpp *cpp, enum token token)
{
	if (cpp->cur->token == token)
		return true;

	cpp_error("%s was expected, got %s",
		token_name(token), token_name(cpp->cur->token));

	return false;
}


static void cpp_match_eol_eof(struct cpp *cpp)
{
	if (!cpp_got(cpp, TOKEN_EOL) && !cpp_got_eof(cpp)) {
		cpp_warn("extra tokens will be skipped");
		cpp_skip_line(cpp);
	}
	else if (cpp_got(cpp, TOKEN_EOL)) {
		cpp_pop(cpp);
	}
}


static void cpp_parse_error(struct cpp *cpp)
{
	cpp_error("%s", cpp->cur->str);
	cpp_pop(cpp);
	cpp_match_eol_eof(cpp);
}


static void cpp_parse_include(struct cpp *cpp)
{
	cpp->lexer.inside_include = true;

	if (cpp_expect(cpp, TOKEN_HEADER_NAME)) {
		DEBUG_PRINTF("#include header file '%s'", cpp->cur->str);
	}

	cpp->lexer.inside_include = false;
}


static bool cpp_expect_directive(struct cpp *cpp)
{
	if (!cpp_expect(cpp, TOKEN_NAME))
		return false;

	if (cpp->cur->symbol->type != SYMBOL_TYPE_CPP_DIRECTIVE) {
		cpp_error("'%s' is not a C preprocessor directive",
			symbol_get_name(cpp->cur->symbol));

		return false;
	}

	return true;
}


static void cpp_skip_ifbranch(struct cpp *cpp);


static void cpp_skip_if(struct cpp *cpp)
{
	DEBUG_TRACE;

	assert(cpp_directive(cpp, CPP_DIRECTIVE_IF)
		|| cpp_directive(cpp, CPP_DIRECTIVE_IFDEF)
		|| cpp_directive(cpp, CPP_DIRECTIVE_IFNDEF)
		|| cpp_directive(cpp, CPP_DIRECTIVE_ELIF)
		|| cpp_directive(cpp, CPP_DIRECTIVE_ELSE));

skip_another_branch:
	if (!cpp_directive(cpp, CPP_DIRECTIVE_ELSE) && !cpp_directive(cpp, CPP_DIRECTIVE_ENDIF))
		cpp_skip_rest_of_directive(cpp); /* ignore the condition */
	else
		cpp_pop(cpp);

	cpp_match_eol_eof(cpp);

	cpp_skip_ifbranch(cpp);

	if (cpp_got_eof(cpp)) {
		cpp_error("unexpected EOF, endif was expected");
		return;
	}

	if (cpp_directive(cpp, CPP_DIRECTIVE_ENDIF)) {
		cpp_pop(cpp);
		cpp_match_eol_eof(cpp);
		return;
	}

	goto skip_another_branch;
}


static void cpp_skip_directive(struct cpp *cpp)
{
	if (!cpp_expect_directive(cpp))
		cpp_skip_line(cpp);

	switch (cpp->cur->symbol->directive) {
	case CPP_DIRECTIVE_IF:
	case CPP_DIRECTIVE_IFDEF:
	case CPP_DIRECTIVE_IFNDEF:
		cpp_skip_if(cpp);
		break;

	default:
		cpp_skip_line(cpp);
	}
}


static void cpp_skip_ifbranch(struct cpp *cpp)
{
	DEBUG_TRACE;

	while (!cpp_got_eof(cpp)) {
		if (!cpp_got_hash(cpp)) {
			cpp_skip_line(cpp);
			continue;
		}

		cpp_pop(cpp);

		if (!cpp_expect_directive(cpp)) {
			cpp_skip_line(cpp);
			return;
		}

		switch (cpp->cur->symbol->directive) {
		case CPP_DIRECTIVE_ELIF:
		case CPP_DIRECTIVE_ELSE:
		case CPP_DIRECTIVE_ENDIF:
		case CPP_DIRECTIVE_IF:
			return;

		default:
			cpp_skip_directive(cpp);
		}
	}
}


static void cpp_parse(struct cpp *cpp);


static void cpp_parse_ifbranch(struct cpp *cpp)
{
	DEBUG_TRACE;
	cpp_parse(cpp);
}


static void cpp_parse_if(struct cpp *cpp)
{
	DEBUG_TRACE;

	assert(cpp_got_eof(cpp)
		|| cpp_directive(cpp, CPP_DIRECTIVE_IF)
		|| cpp_directive(cpp, CPP_DIRECTIVE_ELIF)
		|| cpp_directive(cpp, CPP_DIRECTIVE_ELSE)
		|| cpp_directive(cpp, CPP_DIRECTIVE_ENDIF));

	while (!cpp_got_eof(cpp)) {
		if (cpp_directive(cpp, CPP_DIRECTIVE_ENDIF)) {
			cpp_pop(cpp);
			cpp_skip_line(cpp);
			return;
		}

		if (cpp_directive(cpp, CPP_DIRECTIVE_ELSE)) {
			cpp_pop(cpp);
			cpp_match_eol_eof(cpp);
			cpp_parse_ifbranch(cpp);
			return;
		}

		cpp_skip_ifbranch(cpp);
	}

	cpp_error("EOF was unexpected, expected endif");
}


static void cpp_parse_ifdef(struct cpp *cpp, bool ifndef)
{
	DEBUG_TRACE;

	if (!cpp_expect(cpp, TOKEN_NAME)) {
		cpp_warn("skipping rest of the if directive");
		cpp_skip_if(cpp);
	}
	else if ((cpp->cur->symbol->type == SYMBOL_TYPE_CPP_MACRO) == !ifndef) {
		cpp_pop(cpp);
		cpp_match_eol_eof(cpp);
		cpp_parse_ifbranch(cpp);
	}
	else {
		cpp_pop(cpp);
		cpp_match_eol_eof(cpp);
		cpp_skip_ifbranch(cpp);
		cpp_parse_if(cpp);
	}
}


static void cpp_parse_define(struct cpp *cpp)
{
	DEBUG_TRACE;

	if (!cpp_expect(cpp, TOKEN_NAME)) {
		cpp_skip_line(cpp);
		return;
	}

	cpp->cur->symbol->type = SYMBOL_TYPE_CPP_MACRO;

	cpp_pop(cpp);
	cpp_match_eol_eof(cpp);
}


static void cpp_parse_directive(struct cpp *cpp)
{
	if (!cpp_expect_directive(cpp)) {
		cpp_skip_line(cpp);
		return;
	}

	/*
	 * TODO Right now, I am assuming correct #if..#elif..#else..#endif
	 *      constructs. Add error-handling.
	 */

	switch (cpp->cur->symbol->directive) {
	case CPP_DIRECTIVE_ERROR:
		cpp_pop(cpp);
		cpp_parse_error(cpp);
		break;

	case CPP_DIRECTIVE_INCLUDE:
		cpp_pop(cpp);
		cpp_parse_include(cpp);
		break;

	case CPP_DIRECTIVE_IFDEF:
		cpp_pop(cpp);
		cpp_parse_ifdef(cpp, false);
		break;

	case CPP_DIRECTIVE_IFNDEF:
		cpp_pop(cpp);
		cpp_parse_ifdef(cpp, true);
		break;

	case CPP_DIRECTIVE_DEFINE:
		cpp_pop(cpp);
		cpp_parse_define(cpp);
		break;

	case CPP_DIRECTIVE_ELIF:
	case CPP_DIRECTIVE_ELSE:
		/*
		 * OK, so we must have been parsing an if-branch. If that
		 * branch was activated, others are to be skipped. 
		 *
		 * TODO Validation.
		 */
		cpp_skip_if(cpp);
		break;

	case CPP_DIRECTIVE_ENDIF:
		/*
		 * Same as above.
		 *
		 * TODO Validation.
		 */
		cpp_pop(cpp);
		cpp_match_eol_eof(cpp);
		break;

	default:
		cpp_error("directive %s not supported");
		cpp_skip_line(cpp);
	}
}


static void cpp_parse(struct cpp *cpp)
{
	while (cpp_got_hash(cpp)) {
		cpp_pop(cpp);
		cpp_parse_directive(cpp);
	}
}






mcc_error_t cpp_open(struct cpp *cpp, const char *filename)
{
	mcc_error_t err;

	err = inbuf_open(&cpp->inbuf, 1024, filename);
	if (err != MCC_ERROR_OK)
		return err;

	lexer_set_inbuf(&cpp->lexer, &cpp->inbuf);
	cpp_pop(cpp);

	return MCC_ERROR_OK;
}


void cpp_close(struct cpp *cpp)
{
	inbuf_close(&cpp->inbuf);
}


void cpp_set_symtab(struct cpp *cpp, struct symtab *table)
{
	cpp->table = table;
	lexer_set_symtab(&cpp->lexer, cpp->table);

	cpp_setup_symtab(cpp);
}


struct tokinfo *cpp_next(struct cpp *cpp)
{
	cpp_parse(cpp);
	return cpp_pop(cpp);

}


mcc_error_t cpp_init(struct cpp *cpp)
{
	mcc_error_t err;

	list_init(&cpp->tokens);

	err = lexer_init(&cpp->lexer);
	if (err != MCC_ERROR_OK)
		return err;

	return MCC_ERROR_OK;
}


void cpp_free(struct cpp *cpp)
{
	lexer_free(&cpp->lexer);
	list_free(&cpp->tokens);
}
