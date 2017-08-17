#include "array.h"
#include "parse-internal.h"
#include "parse.h"
#include <stdarg.h>


void parser_setup_symtab(struct symtab *table)
{
	struct symbol *symbol;
	struct symdef *def;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(kwdinfo); i++) {
		symbol = symtab_insert(table, kwdinfo[i].name);

		def = symbol_define(table, symbol);
		def->type = SYMBOL_TYPE_C_KEYWORD;
		def->kwdinfo = &kwdinfo[i];
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


void parse_error(struct parser *parser, char *msg, ...)
{
	(void) parser;

	va_list args;
	va_start(args, msg);
	fprintf(stderr, "parse error: ");
	vfprintf(stderr, msg, args);
	fprintf(stderr, "\n");
	va_end(args);
}


bool parser_require(struct parser *parser, enum token_type token)
{
	if (!parser_expect(parser, token)) {
		parse_error(parser, "%s was expected", token_get_name(token));
		return false;
	}
	return true;
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
	//struct ast_expr *expr;
	//struct strbuf buf;

	cpp_open_file(parser->cpp, cfile);
	parser_next(parser);
	decln = parser_parse_decl(parser);
	//expr = parse_expr(parser);
	cpp_close_file(parser->cpp);

	dump_decln(decln);
	//strbuf_init(&buf, 128);
	//dump_expr(expr, &buf);
	//fprintf(stderr, "%s\n", strbuf_get_string(&buf));
}
