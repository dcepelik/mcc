/*
 * TODO add #pragma and #line support
 * TODO cexpr support in if conditionals
 * TODO stringify and glue support
 */

#include "common.h"
#include "context.h"
#include "debug.h"
#include "inbuf.h"
#include "lexer.h"
#include "macro.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#define FILE_POOL_BLOCK_SIZE	16
#define INBUF_BLOCK_SIZE	2048
#define MACRO_POOL_BLOCK_SIZE	32
#define TOKEN_DATA_BLOCK_SIZE	1024
#define VA_ARGS_NAME		"__VA_ARGS__"


struct cpp_file
{
	struct list_node list_node;
	char *filename;
	struct lexer lexer;
	struct inbuf inbuf;
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
	.token = NULL,
	.skip_this_branch = false,
	.skip_next_branch = true /* no other branches */
};


/*
 * <include> search paths.
 */
static const char *include_dirs[] = {
	".",
	"",
	"/usr/include",
	"../include"
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


struct cpp_file *cpp_cur_file(struct cpp *cpp)
{
	return list_first(&cpp->file_stack);
}


static void cpp_next_token(struct cpp *cpp)
{
	if (!list_is_empty(&cpp->tokens))
		cpp->token = list_remove_first(&cpp->tokens);
	else
		cpp->token = lexer_next(&cpp_cur_file(cpp)->lexer);

	assert(cpp->token != NULL); /* invariant: EOF guards the list */
}


struct cpp *cpp_new(struct context *ctx)
{
	struct cpp *cpp;

	cpp = malloc(sizeof(*cpp));
	if (!cpp)
		return NULL;

	cpp->ctx = ctx;
	cpp->symtab = &cpp->ctx->symtab; /* TODO */
	cpp->token = NULL;

	objpool_init(&cpp->macro_pool, sizeof(struct macro), MACRO_POOL_BLOCK_SIZE);
	objpool_init(&cpp->file_pool, sizeof(struct cpp_file), FILE_POOL_BLOCK_SIZE);

	list_init(&cpp->file_stack);
	list_init(&cpp->tokens);

	list_init(&cpp->ifs);
	list_insert_first(&cpp->ifs, &ifstack_bottom.list_node);

	cpp_setup_symtab(&cpp->ctx->symtab);

	return cpp;
}


static mcc_error_t cpp_file_init(struct cpp *cpp, struct cpp_file *file, char *filename)
{
	mcc_error_t err;

	if ((err = inbuf_open(&file->inbuf, INBUF_BLOCK_SIZE, filename)) != MCC_ERROR_OK)
		goto out;

	if ((err = lexer_init(&file->lexer, cpp->ctx, &file->inbuf)) != MCC_ERROR_OK)
		goto out_inbuf;
	
	file->filename = mempool_strdup(&cpp->ctx->token_data, filename);
	if (!file->filename) {
		err = MCC_ERROR_NOMEM;
		goto out_lexer;
	}

	return MCC_ERROR_OK;

out_lexer:
	lexer_free(&file->lexer);

out_inbuf:
	inbuf_close(&file->inbuf);

out:
	return err;
}


static void cpp_file_free(struct cpp *cpp, struct cpp_file *file)
{
	lexer_free(&file->lexer);
}


static void cpp_file_include(struct cpp *cpp, struct cpp_file *file)
{
	list_insert_first(&cpp->file_stack, &file->list_node);
}


mcc_error_t cpp_open_file(struct cpp *cpp, char *filename)
{
	struct cpp_file *file;
	mcc_error_t err;

	file = objpool_alloc(&cpp->file_pool);
	if (!file)
		return MCC_ERROR_NOMEM;

	if ((err = cpp_file_init(cpp, file, filename)) != MCC_ERROR_OK) {
		objpool_dealloc(&cpp->file_pool, file);
		return err;
	}

	cpp_file_include(cpp, file);
	cpp_next_token(cpp);

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


void cpp_delete(struct cpp *cpp)
{
	objpool_free(&cpp->macro_pool);
	objpool_free(&cpp->file_pool);
	list_free(&cpp->tokens);

	free(cpp);
}


static void cpp_error_internal(struct cpp *cpp, enum error_level level,
	char *fmt, va_list args)
{
	struct strbuf msg;
	strbuf_init(&msg, 64);

	strbuf_vprintf_at(&msg, 0, fmt, args);
	errlist_insert(&cpp->ctx->errlist,
		level,
		"some-file.c",
		strbuf_get_string(&msg),
		strbuf_get_string(&cpp_cur_file(cpp)->lexer.linebuf),
		cpp->token->startloc);

	strbuf_free(&msg);
}


//static void cpp_notice(struct cpp *cpp, char *fmt, ...)
//{
//	va_list args;
//	va_start(args, fmt);
//	cpp_error_internal(cpp, ERROR_LEVEL_NOTICE, fmt, args);
//	va_end(args);
//}


static void cpp_warn(struct cpp *cpp, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	cpp_error_internal(cpp, ERROR_LEVEL_WARNING, fmt, args);
	va_end(args);
}


static void cpp_error(struct cpp *cpp, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	cpp_error_internal(cpp, ERROR_LEVEL_ERROR, fmt, args);
	va_end(args);
}


static void cpp_requeue_current(struct cpp *cpp)
{
	list_insert_first(&cpp->tokens, &cpp->token->list_node);
}


static inline bool cpp_got_hash(struct cpp *cpp)
{
	return token_is(cpp->token, TOKEN_HASH) && cpp->token->is_at_bol;
}


static inline bool cpp_directive(struct cpp *cpp, enum cpp_directive directive)
{
	return token_is(cpp->token, TOKEN_NAME)
		&& cpp->token->symbol->def->type == SYMBOL_TYPE_CPP_DIRECTIVE
		&& cpp->token->symbol->def->directive == directive;
}


static inline void cpp_skip_till_eol(struct cpp *cpp)
{
	/* TODO Consider freeing those tokens */

	assert(cpp_cur_file(cpp)->lexer.emit_eols == true);

	while (!token_is_eol_or_eof(cpp->token))
		cpp_next_token(cpp);

	if (token_is_eol(cpp->token))
		cpp_next_token(cpp);
}


static inline void cpp_skip_find_eol(struct cpp *cpp)
{
	/* TODO Consider freeing those tokens */

	assert(cpp_cur_file(cpp)->lexer.emit_eols == true);

	while (!token_is_eol_or_eof(cpp->token))
		cpp_next_token(cpp);
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

static inline void cpp_match_eol_or_eof(struct cpp *cpp)
{
	if (!token_is(cpp->token, TOKEN_EOL) && !token_is_eof(cpp->token)) {
		cpp_warn(cpp, "extra tokens will be skipped");
		cpp_skip_till_eol(cpp);
	}
	else if (token_is(cpp->token, TOKEN_EOL)) {
		cpp_next_token(cpp);
	}
}


static struct cpp_if *cpp_ifstack_push(struct cpp *cpp, struct token *token)
{
	/* TODO Use a different pool for struct cpp_if? */
	struct cpp_if *cpp_if = mempool_alloc(&cpp->ctx->token_data, sizeof(*cpp_if));
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

	while (!token_is_eol_or_eof(cpp->token)) {
		if (token_is(cpp->token, TOKEN_LPAREN)) {
			cpp_error(cpp, "the '(' may not appear inside macro argument list");
			break;
		}

		if (token_is(cpp->token, TOKEN_COMMA)) {
			if (expect_comma)
				expect_comma = false;
			else
				cpp_error(cpp, "comma was unexpected here");

			cpp_next_token(cpp);
			continue;
		}

		if (token_is(cpp->token, TOKEN_RPAREN)) {
			arglist_ended = true;
			cpp_next_token(cpp);
			break;
		}

		if (expect_comma)
			cpp_error(cpp, "comma was expected here");

		if (cpp->token->type == TOKEN_ELLIPSIS)
			cpp->token->symbol = symtab_search_or_insert(&cpp->ctx->symtab, VA_ARGS_NAME);

		list_insert_last(&macro->args, &cpp->token->list_node);
		expect_comma = true;
		cpp_next_token(cpp);
	}

	if (!arglist_ended)
		cpp_error(cpp, "macro arglist misses terminating )");
}


static void cpp_parse_define(struct cpp *cpp)
{
	struct symdef *symdef;
	struct macro *macro;

	if (!cpp_expect(cpp, TOKEN_NAME)) {
		cpp_skip_till_eol(cpp);
		return;
	}

	if (!cpp_skipping(cpp)) {
		macro = objpool_alloc(&cpp->macro_pool);
		macro_init(macro);
		macro->name = symbol_get_name(cpp->token->symbol);

		symdef = symbol_define(&cpp->ctx->symtab, cpp->token->symbol);
		symdef->type = SYMBOL_TYPE_CPP_MACRO;
		symdef->macro = macro;

		cpp_next_token(cpp);

		if (cpp->token->type == TOKEN_LPAREN && !cpp->token->preceded_by_whitespace) {
			cpp_next_token(cpp);
			cpp_parse_macro_arglist(cpp, macro);
			macro->type = MACRO_TYPE_FUNCLIKE;
			//macro_dump(macro);
		}
		else {
			macro->type = MACRO_TYPE_OBJLIKE;
			//macro_dump(macro);
		}

		while (!token_is_eol_or_eof(cpp->token)) {
			list_insert_last(&macro->expansion, &cpp->token->list_node);
			cpp_next_token(cpp);
		}
	}
	else {
		cpp_next_token(cpp);
	}

	//symtab_dump(&cpp->ctx->symtab, stderr);
}


static void cpp_parse_undef(struct cpp *cpp)
{
	if (!cpp_expect(cpp, TOKEN_NAME)) {
		cpp_skip_till_eol(cpp);
		return;
	}

	if (!cpp_skipping(cpp)) {
		/* TODO check it's defined */
		//TODO symbol_pop_definition(&cpp->ctx->symtab, cpp->token->symbol); 
	}

	cpp_next_token(cpp);
}


static void cpp_parse_error(struct cpp *cpp)
{
	/* TODO cat tokens to get error message */
	cpp_error(cpp, "%s", "#error");
	cpp_skip_find_eol(cpp);
	//cpp_match_eol_or_eof(cpp);
}


/*
 * TODO #include <header> vs #include "header".
 * TODO warn if tokens skipped
 */
static void cpp_parse_include(struct cpp *cpp)
{
	char *filename;
	struct strbuf pathbuf;
	char *path;
	struct cpp_file *file;
	bool file_open = false;
	size_t i;

	if (cpp_skipping(cpp)) {
		cpp_skip_find_eol(cpp);
		goto out;
	}

	if (cpp->token->type != TOKEN_HEADER_HNAME
		&& cpp->token->type != TOKEN_HEADER_QNAME) {
		cpp_error(cpp, "header name was expected, got %s",
			token_get_name(cpp->token->type));

		cpp_skip_find_eol(cpp);
		goto out;
	}

	filename = cpp->token->str;
	strbuf_init(&pathbuf, 128);
	file = objpool_alloc(&cpp->file_pool);

	for (i = 0; i < ARRAY_SIZE(include_dirs); i++) {
		strbuf_reset(&pathbuf);
		strbuf_printf(&pathbuf, "%s/%s", include_dirs[i], filename);

		path = strbuf_get_string(&pathbuf);

		if (cpp_file_init(cpp, file, path) == MCC_ERROR_OK) {
			file_open = true;
			break;
		}
	}

	strbuf_free(&pathbuf);

	if (file_open) {
		cpp_skip_find_eol(cpp); /* get rid of those tokens now */
		cpp_file_include(cpp, file);
	}
	else {
		cpp_error(cpp, "cannot include file %s", filename);
		cpp_skip_find_eol(cpp); /* skip is delayed: error location */
	}

out:
	cpp_cur_file(cpp)->lexer.inside_include = false;
}


static void cpp_parse_directive(struct cpp *cpp)
{
	struct cpp_if *cpp_if;
	struct token *prev;
	enum cpp_directive dir;
	bool skipping;
	bool test_cond;

	if (!cpp_expect_directive(cpp)) {
		cpp_skip_till_eol(cpp);
		return;
	}

	prev = cpp->token;
	dir = cpp->token->symbol->def->directive;

	cpp_cur_file(cpp)->lexer.inside_include
		= cpp_directive(cpp, CPP_DIRECTIVE_INCLUDE);

	cpp_next_token(cpp);

	switch (dir) {
	case CPP_DIRECTIVE_IFDEF:
		test_cond = cpp->token->symbol->def->type == SYMBOL_TYPE_CPP_MACRO;
		cpp_next_token(cpp);
		goto push_if;

	case CPP_DIRECTIVE_IFNDEF:
		test_cond = cpp->token->symbol->def->type != SYMBOL_TYPE_CPP_MACRO;
		cpp_next_token(cpp);
		goto push_if;

	case CPP_DIRECTIVE_IF:
		test_cond = true; /* TODO */

push_if:
		skipping = cpp_skipping(cpp);
		cpp_if = cpp_ifstack_push(cpp, prev);
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
		//symtab_dump(&cpp->ctx->symtab, stderr);
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

	cpp_match_eol_or_eof(cpp);
	cpp_cur_file(cpp)->lexer.emit_eols = false;
}


static void cpp_parse_macro_invocation(struct cpp *cpp)
{
	assert(token_is_macro(cpp->token));

	struct macro *macro;
	struct list invocation;
	struct list expansion;
	unsigned parens_balance = 0;
	bool args_ended = false;

	macro = cpp->token->symbol->def->macro;
	list_init(&invocation);
	list_init(&expansion);

	list_insert_last(&invocation, &cpp->token->list_node); /* macro name */
	cpp_next_token(cpp);

	/*
	 * For invocations of function-like macros, this code block will grab
	 * the whole arglist quickly. If the arglist is malformed, this will
	 * be later reported by the macro module, so no errors are handled here.
	 * This is to avoid duplication of the arglist parsing logic contained
	 * in the macro module.
	 */
	if (macro->type == MACRO_TYPE_FUNCLIKE && token_is(cpp->token, TOKEN_LPAREN)) {
		while (!token_is_eof(cpp->token)) {
			if (token_is(cpp->token, TOKEN_LPAREN)) {
				parens_balance++;
			}
			else if (token_is(cpp->token, TOKEN_RPAREN)) {
				if (--parens_balance == 0)
					args_ended = true;
			}

			list_insert_last(&invocation, &cpp->token->list_node);
			cpp_next_token(cpp);

			if (args_ended)
				break;
		}
	}

	macro_expand(cpp, &invocation, &expansion);
	cpp_requeue_current(cpp);
	list_prepend(&cpp->tokens, &expansion);
	cpp_next_token(cpp);
}


static void cpp_parse(struct cpp *cpp)
{
	while (!token_is_eof(cpp->token)) {
		if (cpp_got_hash(cpp)) {
			cpp_cur_file(cpp)->lexer.emit_eols = true;
			cpp_next_token(cpp);
			cpp_parse_directive(cpp);

		}
		else if (cpp->token->type == TOKEN_NAME
			&& cpp->token->symbol->def->type == SYMBOL_TYPE_CPP_MACRO
			&& !cpp->token->noexpand) {

			/* TODO no cpp_next_token(cpp) here */
			if (cpp->token->symbol->def->macro->type == MACRO_TYPE_FUNCLIKE) {
				struct token *macro_name = cpp->token;
				cpp_next_token(cpp);

				if (cpp->token->type == TOKEN_LPAREN) {
					list_insert_first(&cpp->tokens, &cpp->token->list_node);
					cpp->token = macro_name;
					cpp_parse_macro_invocation(cpp);
				}
				else {
					macro_name->noexpand = true;
					cpp->token = macro_name;
					list_insert_first(&cpp->tokens, &cpp->token->list_node);
				}

			}
			else {
				cpp_parse_macro_invocation(cpp);
			}
		}
		else {
			if (!cpp_skipping(cpp))
				break;

			cpp_next_token(cpp);
		}
	}
}


struct token *cpp_next(struct cpp *cpp)
{
	assert(!list_is_empty(&cpp->file_stack));

	struct token *tmp;

again:
	cpp_parse(cpp);

	if (token_is_eof(cpp->token)) {
		if (list_length(&cpp->file_stack) == 1)
			return cpp->token;

		cpp_close_file(cpp);
		cpp_next_token(cpp);
		goto again;
	}

	tmp = cpp->token;
	cpp_next_token(cpp);
	return tmp;
}
