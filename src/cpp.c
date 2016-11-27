#include "array.h"
#include "common.h"
#include "debug.h"
#include "symtab.h"
#include <assert.h>


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




static void cpp_push_token(struct cpp *cpp, struct tokinfo *tokinfo)
{
	list_insert_first(&cpp->tokens, &tokinfo->list_node);
}


static struct tokinfo *cpp_pop_token(struct cpp *cpp)
{
	struct tokinfo *tokinfo;

	if (list_length(&cpp->tokens) == 0) {
		tokinfo = lexer_next(&cpp->lexer);

		if (tokinfo)
			list_insert_last(&cpp->tokens, &tokinfo->list_node);
	}

	assert(list_length(&cpp->tokens) > 0); /* at least TOKEN_EOF/TOKEN_EOL */

	return (cpp->cur = list_remove_first(&cpp->tokens));
}


static bool cpp_cur_is(struct cpp *cpp, enum token token)
{
	return cpp->cur->token == token;
}


static void cpp_skip_line(struct cpp *cpp)
{
	while (cpp_pop_token(cpp)) {
		if (cpp->cur->token == TOKEN_EOL || cpp->cur->token == TOKEN_EOF)
			break;
	}
}


static void cpp_require_eol(struct cpp *cpp)
{
	cpp_pop_token(cpp);
	if (cpp->cur->token != TOKEN_EOL && cpp->cur->token != TOKEN_EOF) {
		DEBUG_MSG("warning: extra tokens will be skipped");
		cpp_skip_line(cpp);
	}
}




static void cpp_handle_error(struct cpp *cpp)
{
	DEBUG_MSG("#error"); /* TODO */
	cpp_skip_line(cpp);
}


static void cpp_handle_include(struct cpp *cpp)
{
	cpp->lexer.inside_include = true;

	cpp_pop_token(cpp);

	if (cpp->cur->token != TOKEN_HEADER_NAME) {
		DEBUG_MSG("error: header name expected");
	}
	else {
		DEBUG_PRINTF("#include header file '%s'", cpp->cur->str);
	}

	cpp->lexer.inside_include = false;
}


static bool cpp_is_valid_directive(struct cpp *cpp)
{
	if (!cpp_cur_is(cpp, TOKEN_NAME)) {
		DEBUG_MSG("error: directive name was expected");
		return false;
	}

	if (cpp->cur->symbol->type != SYMBOL_TYPE_CPP_DIRECTIVE) {
		DEBUG_PRINTF("error: '%s' is not a C preprocessor directive",
			symbol_get_name(cpp->cur->symbol));

		return false;
	}

	return true;
}


static bool cpp_directive_follows(struct cpp *cpp)
{
	return cpp->cur->token == TOKEN_HASH && cpp->cur->is_at_bol;
}


static void cpp_skip_branch(struct cpp *cpp);


static void cpp_skip_if(struct cpp *cpp)
{
skip_elif_branch:
	cpp_skip_line(cpp); /* ignore the condition */

skip_else_branch:
	cpp_skip_branch(cpp);

	cpp_pop_token(cpp);
	assert(cpp_directive_follows(cpp));

	cpp_pop_token(cpp);
	assert(cpp_is_valid_directive(cpp));

	switch (cpp->cur->symbol->directive) {
	case CPP_DIRECTIVE_ELIF:
		goto skip_elif_branch;

	case CPP_DIRECTIVE_ELSE:
		goto skip_else_branch;

	case CPP_DIRECTIVE_ENDIF:
		cpp_require_eol(cpp);
		return;

	default:
		assert(false);
	}

	DEBUG_MSG("error: #endif was expected here");
}


static void cpp_skip_directive(struct cpp *cpp)
{
	if (!cpp_is_valid_directive(cpp))
		cpp_skip_line(cpp);

	switch (cpp->cur->symbol->directive) {
	case CPP_DIRECTIVE_IF:
	case CPP_DIRECTIVE_IFNDEF:
	case CPP_DIRECTIVE_IFDEF:
		cpp_skip_if(cpp);
		break;

	default:
		cpp_skip_line(cpp);
	}
}


static void cpp_skip_branch(struct cpp *cpp)
{
	DEBUG_TRACE;

	while (cpp_pop_token(cpp)) {
		if (cpp_directive_follows(cpp)) {
			cpp_pop_token(cpp);

			if (cpp_is_valid_directive(cpp)) {
				switch (cpp->cur->symbol->directive) {
				case CPP_DIRECTIVE_IF:
				case CPP_DIRECTIVE_ELSE:
				case CPP_DIRECTIVE_ELIF:
					cpp_push_token(cpp, cpp->cur);
					return;

				default:
					cpp_skip_directive(cpp);
				}
			}
			else {
				cpp_skip_line(cpp);
			}
		}
		else {
			if (cpp_cur_is(cpp, TOKEN_EOF))
				return;
		}
	}
}


static void cpp_handle_ifndef(struct cpp *cpp)
{
	DEBUG_TRACE;

	cpp_pop_token(cpp);
	if (!cpp_cur_is(cpp, TOKEN_NAME)) {
		DEBUG_MSG("error: macro name expected");
		cpp_skip_line(cpp);
		return;
	}

	if (cpp->cur->symbol->type != SYMBOL_TYPE_CPP_MACRO) {
		/* merge */
		DEBUG_MSG("not defined, merging");
	}
	else {
		DEBUG_MSG("defined, skipping");
		cpp_skip_branch(cpp);
	}
}


static void cpp_handle_define(struct cpp *cpp)
{
	DEBUG_TRACE;

	cpp_pop_token(cpp);
	if (!cpp_cur_is(cpp, TOKEN_NAME)) {
		DEBUG_MSG("error: macro name expected");
		cpp_skip_line(cpp);
	}

	cpp->cur->symbol->type = SYMBOL_TYPE_CPP_MACRO;

	cpp_require_eol(cpp);
}


static void cpp_handle_directive(struct cpp *cpp)
{
	cpp_pop_token(cpp);

	if (!cpp_is_valid_directive(cpp)) {
		cpp_skip_line(cpp);
		return;
	}

	switch (cpp->cur->symbol->directive) {
	case CPP_DIRECTIVE_ERROR:
		cpp_handle_error(cpp);
		break;

	case CPP_DIRECTIVE_INCLUDE:
		cpp_handle_include(cpp);
		break;

	case CPP_DIRECTIVE_IFNDEF:
		cpp_handle_ifndef(cpp);
		break;

	case CPP_DIRECTIVE_DEFINE:
		cpp_handle_define(cpp);
		break;

	default:
		DEBUG_PRINTF("error: directive %s not supported",
			symbol_get_name(cpp->cur->symbol));
		cpp_skip_line(cpp);
	}

	if (cpp_cur_is(cpp, TOKEN_EOL) && cpp_cur_is(cpp, TOKEN_EOF)) {
		DEBUG_MSG("warning: extra (unprocessed) tokens after directive");
		cpp_skip_line(cpp);
	}
}







mcc_error_t cpp_open(struct cpp *cpp, const char *filename)
{
	mcc_error_t err;

	err = inbuf_open(&cpp->inbuf, 1024, filename);
	if (err != MCC_ERROR_OK)
		return err;

	lexer_set_inbuf(&cpp->lexer, &cpp->inbuf);

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
	cpp_pop_token(cpp);

	while (cpp_directive_follows(cpp)) {
		cpp_handle_directive(cpp);
		cpp_pop_token(cpp);
	}

	return cpp->cur;

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

