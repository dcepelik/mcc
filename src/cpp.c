#include "common.h"
#include "cpp.h"
#include "debug.h"
#include "symtab.h"


struct directive
{
	const char *name;
	void (*handler)(void);
};


static void cpp_missing_handler(void)
{
	DEBUG_MSG("Missing directive handler");
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
		{ .name = "error", .handler = cpp_missing_handler },
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


mcc_error_t cpp_init(struct cpp *cpp)
{
	return lexer_init(&cpp->lexer);
}


void cpp_free(struct cpp *cpp)
{
	lexer_free(&cpp->lexer);
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


mcc_error_t cpp_next(struct cpp *cpp, struct tokinfo *tokinfo)
{
	mcc_error_t err;

	err = lexer_next(&cpp->lexer, tokinfo);
	if (err != MCC_ERROR_OK)
		return err;

	switch (tokinfo->token)
	{
	case TOKEN_HASH:
		break;

	default:
		break;
	}

	return err;
}
