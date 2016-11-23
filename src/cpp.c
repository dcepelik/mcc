#include "array.h"
#include "common.h"
#include "cpp.h"
#include "debug.h"
#include "symtab.h"
#include <assert.h>


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

	if (cpp->cur[0]->token != TOKEN_NAME) {
		DEBUG_MSG("error: expected directive name");
		// TODO Invalid pp line, read till EOL
		cpp->cur++;
		return *cpp->cur;
	}

	if (cpp->cur[0]->symbol->type != SYMBOL_TYPE_CPP_DIRECTIVE) {
		DEBUG_PRINTF("error: %s is not a C preprocessor directive",
			symbol_get_name(cpp->cur[0]->symbol));
		cpp->cur++;
		return *cpp->cur;
	}

	return *cpp->cur;
}


static bool cpp_load_line_of_tokens(struct cpp *cpp)
{
	struct tokinfo *tokinfo;
	size_t i;

	array_reset(cpp->tokens);

	for (i = 0; (tokinfo = lexer_next(&cpp->lexer)); i++) {
		cpp->tokens = array_claim(cpp->tokens, 1);
		if (!cpp->tokens)
			return false;

		cpp->tokens[i] = tokinfo;

		if (tokinfo->token == TOKEN_EOL || tokinfo->token == TOKEN_EOF)
			break;
	}

	cpp->cur = cpp->tokens;

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
	if (cpp->cur == cpp->tokens + array_size(cpp->tokens))
		cpp_load_line_of_tokens(cpp);

	assert(array_size(cpp->tokens) > 0);

	switch (cpp->cur[0]->token) {
	case TOKEN_EOF:
		return *cpp->cur; /* TOKEN_EOF not edible */

	case TOKEN_HASH:
		if (cpp->cur[0]->is_at_bol) {
			cpp->cur++;
			return cpp_directive(cpp);
		}

		break;

	default:
		return *cpp->cur++;
	}
	
	return *cpp->cur++;
}


mcc_error_t cpp_init(struct cpp *cpp)
{
	mcc_error_t err;

	cpp->tokens = array_new(16, sizeof(struct tokinfo));
	if (!cpp->tokens)
		return MCC_ERROR_NOMEM;

	cpp->cur = cpp->tokens;

	err = lexer_init(&cpp->lexer);
	if (err != MCC_ERROR_OK)
		return err;

	return MCC_ERROR_OK;
}


void cpp_free(struct cpp *cpp)
{
	lexer_free(&cpp->lexer);
	array_delete(cpp->tokens);
}

