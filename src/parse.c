#include "parse.h"
#include "array.h"

#include "parse-internal.h"


void parser_setup_symtab(struct symtab *table)
{
	struct symbol *symbol;
	struct symdef *def;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(keywords); i++) {
		symbol = symtab_insert(table, keywords[i].name);

		def = symbol_define(table, symbol);
		def->type = SYMBOL_TYPE_C_KEYWORD;
		def->keyword = &keywords[i];
	}
}


void parser_init(struct parser *parser)
{
	context_init(&parser->ctx);
	parser_setup_symtab(&parser->ctx.symtab);
	parser->cpp = cpp_new(&parser->ctx);
}


void parser_free(struct parser *parser)
{
	context_free(&parser->ctx);
	cpp_delete(parser->cpp);
}


void parser_next(struct parser *parser)
{
	parser->token = cpp_next(parser->cpp);
}


void parser_skip(struct parser *parser)
{
	/* TODO free */
	(void) parser_next(parser);
}


void parser_next_push(struct parser *parser, struct toklist *toklist)
{
	toklist_insert_last(toklist, parser->token);
	parser_next(parser);
}

bool parser_is_eof(struct parser *parser)
{
	return token_is(parser->token, TOKEN_EOF);
}


void parser_skip_rest(struct parser *parser)
{
	(void) parser;
}


bool parser_expect(struct parser *parser, enum token_type type)
{
	if (token_is(parser->token, type)) {
		parser_next(parser);
		return true;
	}

	return false;
}


void parser_build_ast(struct parser *parser, struct ast *tree, char *cfile)
{
	(void) tree;

	struct ast_node *decln;

	cpp_open_file(parser->cpp, cfile);
	parser_next(parser);
	decln = parser_parse_decl(parser);
	cpp_close_file(parser->cpp);

	dump_decln(decln);
	fprintf(stderr, "\n");
}