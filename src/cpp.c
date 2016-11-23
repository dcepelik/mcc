#include "array.h"
#include "common.h"
#include "cpp.h"
#include "debug.h"
#include "symtab.h"
#include <assert.h>


/*
 * TODO Assuming that list_node is first member of struct tokinfo.
 *      Define a container_of macro to drop this requirement.
 */


struct directive
{
	const char *name;
	void (*handler)(void);
};


static void cpp_missing_handler(void)
{
	DEBUG_MSG("Missing directive handler");
}


static void cpp_directive_error(void)
{
	DEBUG_MSG("#error");
}


static void cpp_setup_symtab_directives(struct cpp *cpp)
{
	struct symbol *symbol;
	size_t i;

	const struct directive directives[] = {
		{ .name = "if", .handler = cpp_missing_handler },
		{ .name = "ifdef", .handler = cpp_missing_handler },
		{ .name = "ifndef", .handler = cpp_missing_handler },
		{ .name = "elif", .handler = cpp_missing_handler },
		{ .name = "else", .handler = cpp_missing_handler },
		{ .name = "endif", .handler = cpp_missing_handler },
		{ .name = "include", .handler = cpp_missing_handler },
		{ .name = "define", .handler = cpp_missing_handler },
		{ .name = "undef", .handler = cpp_missing_handler },
		{ .name = "line", .handler = cpp_missing_handler },
		{ .name = "error", .handler = cpp_directive_error },
		{ .name = "pragma", .handler = cpp_missing_handler },
	};

	for (i = 0; i < ARRAY_SIZE(directives); i++) {
		symbol = symtab_insert(cpp->table, directives[i].name);
		symbol->type = SYMBOL_TYPE_CPP_DIRECTIVE;
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


static struct tokinfo *cpp_directive(struct cpp *cpp)
{
	DEBUG_TRACE;

	struct tokinfo *cur = list_first(&cpp->tokens);

	if (cur->token != TOKEN_NAME) {
		DEBUG_MSG("error: expected directive name");
		// TODO Invalid pp line, read till EOL

		list_remove_first(&cpp->tokens);
		return cur;

	}

	if (cur->symbol->type != SYMBOL_TYPE_CPP_DIRECTIVE) {
		DEBUG_PRINTF("error: %s is not a C preprocessor directive",
			symbol_get_name(cur->symbol));

		list_remove_first(&cpp->tokens);
		return cur;
	}

	return cur;
}


static bool cpp_load_line_of_tokens(struct cpp *cpp)
{
	struct tokinfo *tokinfo;

	while ((tokinfo = lexer_next(&cpp->lexer))) {
		list_insert_last(&cpp->tokens, &tokinfo->list_node);

		if (tokinfo->token == TOKEN_EOL || tokinfo->token == TOKEN_EOF)
			break;
	}

	return tokinfo != NULL; /* lexer_next did not fail */
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
	struct tokinfo *cur;

	if (list_length(&cpp->tokens) == 0)
		cpp_load_line_of_tokens(cpp);

	assert(list_length(&cpp->tokens) > 0); /* at least TOKEN_EOF/TOKEN_EOL */

	cur = list_first(&cpp->tokens);

	switch (cur->token) {
	case TOKEN_EOF:
		return cur; /* TOKEN_EOF not edible */

	case TOKEN_HASH:
		if (cur->is_at_bol) {
			list_remove_first(&cpp->tokens);
			return cpp_directive(cpp);
		}

		break;

	default:
		list_remove_first(&cpp->tokens);
		return cur;
	}
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

