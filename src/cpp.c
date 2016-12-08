/*
 * TODO Add #pragma and #line support.
 */

#include "common.h"
#include "debug.h"
#include "macro.h"
#include "lexer.h"
#include "symbol.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#define TOKEN_POOL_BLOCK_SIZE	256
#define MACRO_POOL_BLOCK_SIZE	32
#define TOKEN_DATA_BLOCK_SIZE	1024
#define FILE_POOL_BLOCK_SIZE	16


struct cpp_file
{
	struct list_node list_node;
	struct lexer lexer;
};


struct cpp_if
{
	struct list_node list_node;
	struct token *token;
	bool skip_this_branch;
	bool skip_next_branch;
};


/*
 * Artificial item to be kept at the bottom of the if stack to avoid special
 * cases in the code. Its presence is equivalent to the whole file being
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


static void cpp_setup_symtab_directives(struct symtab *table)
{
	struct symbol *symbol;
	struct symdef *symdef;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(directives); i++) {
		symbol = symtab_insert(table, directives[i].name);

		symdef = symbol_define(table, symbol);
		symdef->type = SYMBOL_TYPE_CPP_DIRECTIVE;
		symdef->directive = directives[i].directive;
		symdef->symbol = symbol;
	}
}


static void cpp_setup_symtab_builtins(struct symtab *table)
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
		symbol = symtab_insert(table, builtins[i]);

		symdef = symbol_define(table, symbol);
		symdef->type = SYMBOL_TYPE_CPP_BUILTIN;
	}
}


static void cpp_setup_symtab(struct symtab *table)
{
	cpp_setup_symtab_directives(table);
	cpp_setup_symtab_builtins(table);
}


struct cpp *cpp_new()
{
	struct cpp *cpp;

	cpp = malloc(sizeof(*cpp));

	mempool_init(&cpp->token_data, TOKEN_DATA_BLOCK_SIZE);
	objpool_init(&cpp->token_pool, sizeof(struct token), TOKEN_POOL_BLOCK_SIZE);
	objpool_init(&cpp->macro_pool, sizeof(struct macro), MACRO_POOL_BLOCK_SIZE);
	objpool_init(&cpp->file_pool, sizeof(struct cpp_file), FILE_POOL_BLOCK_SIZE);

	list_init(&cpp->file_stack);
	list_init(&cpp->tokens);
	cpp->token = NULL;
	list_init(&cpp->ifs);

	cpp->symtab = malloc(sizeof(*cpp->symtab));
	symtab_init(cpp->symtab);
	cpp_setup_symtab(cpp->symtab);

	return cpp;
}


void cpp_delete(struct cpp *cpp)
{
	mempool_free(&cpp->token_data);
	objpool_free(&cpp->token_pool);
	objpool_free(&cpp->macro_pool);
	objpool_free(&cpp->file_pool);
	list_free(&cpp->tokens);

	free(cpp);
}


mcc_error_t cpp_open_file(struct cpp *cpp, char *filename)
{
	struct cpp_file *file;
	mcc_error_t err;

	file = objpool_alloc(&cpp->file_pool);

	err = lexer_init(&file->lexer, filename);
	if (err != MCC_ERROR_OK)
		return err;

	list_insert_first(&cpp->file_stack, &file->list_node);

	return MCC_ERROR_OK;
}


void cpp_close_file(struct cpp *cpp)
{
	assert(!list_is_empty(&cpp->file_stack));

	struct cpp_file *file;

	file = list_first(&cpp->file_stack);
	lexer_free(&file->lexer);
	list_remove_first(&cpp->file_stack);

	objpool_dealloc(&cpp->file_pool, file);
}


struct cpp_file *cpp_cur_file(struct cpp *cpp)
{
	return list_first(&cpp->file_stack);
}


void cpp_error(struct cpp *cpp, char *fmt, ...)
{
	struct strbuf buf;
	char *line;
	size_t i;
	struct location loc;

	(void) cpp;

	line = strbuf_get_string(&cpp_cur_file(cpp)->lexer.linebuf);
	loc = cpp->token->startloc;

	va_list args;
	va_start(args, fmt);
	strbuf_init(&buf, 1024);

	strbuf_printf(&buf, "error: ");
	strbuf_vprintf_at(&buf, strbuf_strlen(&buf), fmt, args);
	strbuf_printf(&buf, "\n%s\n", line);

	for (i = 0; i < loc.column_no; i++) {
		if (line[i] == '\t')
			strbuf_putc(&buf, '\t');
		else
			strbuf_putc(&buf, ' ');
	}

	strbuf_printf(&buf, "^^^");

	fprintf(stderr, "%s\n", strbuf_get_string(&buf));

	va_end(args);
	strbuf_free(&buf);
}


static struct token *cpp_pop(struct cpp *cpp)
{
	struct cpp_file *file;
	struct token *token;
	struct token *tmp;

	if (list_is_empty(&cpp->tokens)) {
		file = cpp_cur_file(cpp);

		token = lexer_next(cpp, &file->lexer);
		assert(token != NULL);

		list_insert_last(&cpp->tokens, &token->list_node);
	}

	assert(!list_is_empty(&cpp->tokens)); /* at least TOKEN_EOF/TOKEN_EOL */

	tmp = cpp->token;
	cpp->token = list_remove_first(&cpp->tokens);
	return tmp;
}


static inline bool cpp_got(struct cpp *cpp, enum token_type token)
{
	return cpp->token->type == token;
}


static inline bool cpp_got_eol(struct cpp *cpp)
{
	return cpp_got(cpp, TOKEN_EOL);
}


static inline bool cpp_got_eof(struct cpp *cpp)
{
	return cpp_got(cpp, TOKEN_EOF);
}


static inline bool cpp_got_eol_eof(struct cpp *cpp)
{
	return cpp_got_eol(cpp) || cpp_got_eof(cpp);
}


static inline bool cpp_got_hash(struct cpp *cpp)
{
	return cpp_got(cpp, TOKEN_HASH) && cpp->token->is_at_bol;
}


static inline bool cpp_directive(struct cpp *cpp, enum cpp_directive directive)
{
	return cpp_got(cpp, TOKEN_NAME)
		&& cpp->token->symbol->def->type == SYMBOL_TYPE_CPP_DIRECTIVE
		&& cpp->token->symbol->def->directive == directive;
}


static inline void cpp_skip_line(struct cpp *cpp)
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


static inline bool cpp_expect(struct cpp *cpp, enum token_type token)
{
	if (cpp->token->type == token)
		return true;

	cpp_error(cpp, "%s was expected, got %s",
		token_get_name(token), token_get_name(cpp->token->type));

	return false;
}


static inline bool cpp_expect_directive(struct cpp *cpp)
{
	if (!cpp_expect(cpp, TOKEN_NAME))
		return false;

	if (cpp->token->symbol->def->type != SYMBOL_TYPE_CPP_DIRECTIVE) {
		cpp_error(cpp, "'%s' is not a C preprocessor directive",
			symbol_get_name(cpp->token->symbol));

		return false;
	}

	return true;
}

static inline void cpp_match_eol_eof(struct cpp *cpp)
{
	if (!cpp_got(cpp, TOKEN_EOL) && !cpp_got_eof(cpp)) {
		cpp_error(cpp, "extra tokens will be skipped");
		cpp_skip_line(cpp);
	}
	else if (cpp_got(cpp, TOKEN_EOL)) {
		cpp_pop(cpp);
	}
}


static struct cpp_if *cpp_ifstack_push(struct cpp *cpp, struct token *token)
{
	struct cpp_if *cpp_if = mempool_alloc(&cpp->token_data, sizeof(*cpp_if));
	cpp_if->token = token;
	cpp_if->skip_this_branch = false;
	cpp_if->skip_next_branch = false;

	list_insert_first(&cpp->ifs, &cpp_if->list_node);

	return cpp_if;
}


static struct cpp_if *cpp_ifstack_pop(struct cpp *cpp)
{
	return list_remove_first(&cpp->ifs);
}


static struct cpp_if *cpp_ifstack_top(struct cpp *cpp)
{
	return list_first(&cpp->ifs);
}


static inline bool cpp_skipping(struct cpp *cpp)
{
	return cpp_ifstack_top(cpp)->skip_this_branch;
}


static void cpp_parse_macro_arglist(struct cpp *cpp, struct macro *macro)
{
	bool expect_comma = false;
	bool arglist_ended = false;

	while (!cpp_got_eol_eof(cpp)) {
		if (cpp_got(cpp, TOKEN_LPAREN)) {
			cpp_error(cpp, "the '(' may not appear inside macro argument list");
			break;
		}

		if (cpp_got(cpp, TOKEN_COMMA)) {
			if (expect_comma)
				expect_comma = false;
			else
				cpp_error(cpp, "comma was unexpected here");

			cpp_pop(cpp);
			continue;
		}

		if (cpp_got(cpp, TOKEN_RPAREN)) {
			arglist_ended = true;
			cpp_pop(cpp);
			break;
		}

		if (expect_comma)
			cpp_error(cpp, "comma was expected here");

		list_insert_last(&macro->args, &cpp->token->list_node);
		expect_comma = true;
		cpp_pop(cpp);
	}

	if (!arglist_ended)
		cpp_error(cpp, "macro arglist misses terminating )");
}


static void cpp_parse_define(struct cpp *cpp)
{
	struct symdef *symdef;
	struct macro *macro;
	struct token *tmp;

	if (!cpp_expect(cpp, TOKEN_NAME)) {
		cpp_skip_line(cpp);
		return;
	}

	if (!cpp_skipping(cpp)) {
		macro = objpool_alloc(&cpp->macro_pool);
		macro_init(macro);
		macro->name = symbol_get_name(cpp->token->symbol);

		symdef = symbol_define(cpp->symtab, cpp->token->symbol);
		symdef->type = SYMBOL_TYPE_CPP_MACRO;
		symdef->macro = macro;

		cpp_pop(cpp);

		if (cpp->token->type == TOKEN_LPAREN && !cpp->token->preceded_by_whitespace) {
			cpp_pop(cpp);
			cpp_parse_macro_arglist(cpp, macro);
			macro->type = MACRO_TYPE_FUNCLIKE;
		}
		else {
			macro->type = MACRO_TYPE_OBJLIKE;
		}

		while (!cpp_got_eol_eof(cpp)) {
			tmp = cpp_pop(cpp);
			list_insert_last(&macro->expansion, &tmp->list_node);
		}
	}
	else {
		cpp_pop(cpp);
	}

	//macro_dump(macro);
}


static void cpp_parse_undef(struct cpp *cpp)
{
	if (!cpp_expect(cpp, TOKEN_NAME)) {
		cpp_skip_line(cpp);
		return;
	}

	if (!cpp_skipping(cpp)) {
		/* TODO check it's defined */
		//TODO symbol_pop_definition(cpp->symtab, cpp->token->symbol); 
	}

	cpp_pop(cpp);
}


static void cpp_parse_error(struct cpp *cpp)
{
	cpp_error(cpp, "%s", cpp->token->str);
	cpp_pop(cpp);
	cpp_match_eol_eof(cpp);
}


/*
 * TODO #include <header> vs #include "header".
 */
static void cpp_parse_include(struct cpp *cpp)
{
	char *filename;

	if (cpp_expect(cpp, TOKEN_HEADER_NAME) && !cpp_skipping(cpp)) {
		filename = cpp->token->str;

		/* TODO warning if tokens thrown away */
		cpp_skip_rest_of_directive(cpp);

		cpp_cur_file(cpp)->lexer.inside_include = false;

		if (cpp_open_file(cpp, filename) != MCC_ERROR_OK) {
			cpp_error(cpp, "cannot include file %s: access denied",
				filename);
		}
	}

	cpp_pop(cpp);
}


static void cpp_parse_directive(struct cpp *cpp)
{
	struct cpp_if *cpp_if;
	enum cpp_directive dir;
	bool skipping;
	bool test_cond;

	if (!cpp_expect_directive(cpp)) {
		cpp_skip_line(cpp);
		return;
	}

	dir = cpp->token->symbol->def->directive;

	cpp_cur_file(cpp)->lexer.inside_include
		= cpp_directive(cpp, CPP_DIRECTIVE_INCLUDE);

	cpp_pop(cpp);

	switch (dir) {
	case CPP_DIRECTIVE_IFDEF:
		test_cond = cpp->token->symbol->def->type == SYMBOL_TYPE_CPP_MACRO;
		cpp_pop(cpp);
		goto push_if;

	case CPP_DIRECTIVE_IFNDEF:
		test_cond = cpp->token->symbol->def->type != SYMBOL_TYPE_CPP_MACRO;
		cpp_pop(cpp);
		goto push_if;

	case CPP_DIRECTIVE_IF:
		test_cond = true; /* TODO */

push_if:
		skipping = cpp_skipping(cpp);
		cpp_if = cpp_ifstack_push(cpp, cpp->token);
		cpp_if->skip_next_branch = skipping;

		goto conclude;

	case CPP_DIRECTIVE_ELIF:
		test_cond = true; /* TODO */
		cpp_if = cpp_ifstack_top(cpp);

conclude:
		cpp_if->skip_this_branch = !test_cond || cpp_if->skip_next_branch;
		cpp_if->skip_next_branch |= !cpp_if->skip_this_branch;

		break;
	
	case CPP_DIRECTIVE_ELSE:
		/* TODO check elif after else */
		cpp_if = cpp_ifstack_top(cpp);
		cpp_if->skip_this_branch = cpp_if->skip_next_branch;
		break;

	case CPP_DIRECTIVE_ENDIF:
		assert(cpp_ifstack_top(cpp) != &ifstack_bottom); /* TODO */

		if (cpp_ifstack_top(cpp) == &ifstack_bottom)
			cpp_error(cpp, "endif without matching if/ifdef/ifndef");
		else
			cpp_ifstack_pop(cpp);

		break;

	case CPP_DIRECTIVE_DEFINE:
		cpp_parse_define(cpp);
		break;

	case CPP_DIRECTIVE_INCLUDE:
		cpp_parse_include(cpp);

		/*
		 * TODO To keep tokens after the #include "filename" (if any).
		 *      How to get rid of this?
		 */
		return;

	case CPP_DIRECTIVE_ERROR:
		cpp_parse_error(cpp);
		break;

	case CPP_DIRECTIVE_UNDEF:
		cpp_parse_undef(cpp);
		break;

	default:
		return;
	}

	cpp_match_eol_eof(cpp);
	//cpp_cur_file(cpp)->lexer.emit_eols = false;
}


void cpp_dump_toklist(struct list *lst, FILE *fout)
{
	struct strbuf buf;

	strbuf_init(&buf, 1024);
	strbuf_printf(&buf, "TL: ");

	int c = 0;

	list_foreach(struct token, token, lst, list_node) {
		c++;
		token_print(token, &buf);
		if (token != list_last(lst))
			strbuf_putc(&buf, ' ');

		if (c > 100) {
			fprintf(stderr, "*** MAYBE RECURSION ***\n");
			break;
		}
	}

	fprintf(fout, "%s\n", strbuf_get_string(&buf));

	strbuf_free(&buf);
}


static void cpp_expand_macro(struct cpp *cpp)
{
	struct macro *macro;
	struct list invocation;
	struct list expansion;
	unsigned parens_balance = 0;

	list_init(&invocation);
	list_init(&expansion);

	assert(cpp_got(cpp, TOKEN_NAME));
	assert(cpp->token->symbol->def->type == SYMBOL_TYPE_CPP_MACRO);

	macro = cpp->token->symbol->def->macro;
	list_insert_last(&invocation, &cpp_pop(cpp)->list_node); /* macro name */

	/*
	 * For invocations of function-like macros, this code block will grab
	 * the whole arglist quickly. If the arglist is malformed, this will
	 * be later reported by the macro module, so no errors are handled here.
	 * This is to avoid duplication of the arglist parsing logic contained
	 * in the macro module.
	 */
	if (macro->type == MACRO_TYPE_FUNCLIKE) {
		while (!cpp_got_eof(cpp)) {
			if (cpp_got(cpp, TOKEN_LPAREN)) {
				parens_balance++;
			}
			else if (cpp_got(cpp, TOKEN_RPAREN)) {
				parens_balance--;

				if (parens_balance == 0) {
					list_insert_last(&invocation, &cpp_pop(cpp)->list_node);
					break;
				}
			}

			list_insert_last(&invocation, &cpp_pop(cpp)->list_node);
		}
	}

	//cpp_dump_toklist(&invocation);
	macro_expand(cpp, &invocation, &expansion);

	list_insert_first(&cpp->tokens, &cpp->token->list_node);
	list_prepend(&cpp->tokens, &expansion);
	cpp_pop(cpp);
}


static void cpp_parse(struct cpp *cpp)
{
	if (cpp->token == NULL) { /* TODO Get rid of this special case */
		cpp_pop(cpp); 
		list_insert_first(&cpp->ifs, &ifstack_bottom.list_node);
	}

	while (!cpp_got_eof(cpp)) {
		if (cpp_got_hash(cpp)) {
			cpp_cur_file(cpp)->lexer.emit_eols = true;
			cpp_pop(cpp);
			cpp_parse_directive(cpp);

		}
		else if (cpp->token->type == TOKEN_NAME
			&& cpp->token->symbol->def->type == SYMBOL_TYPE_CPP_MACRO
			&& !cpp->token->noexpand) {
			/* exception: no cpp_pop(cpp) here */
			cpp_expand_macro(cpp);
		}
		else {
			if (!cpp_skipping(cpp))
				break;

			cpp_pop(cpp);
		}
	}
}


struct token *cpp_next(struct cpp *cpp)
{
	assert(!list_is_empty(&cpp->file_stack));

again:
	cpp_parse(cpp);

	if (cpp_got_eof(cpp)) {
		if (list_length(&cpp->file_stack) == 1)
			return cpp->token;

		cpp_close_file(cpp);
		cpp_pop(cpp);
		goto again;
	}

	return cpp_pop(cpp);
}
