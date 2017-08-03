/*
 * TODO add #pragma and #line support
 * TODO cexpr support in if conditionals
 */

#include "cpp-internal.h"
#include "context.h"
#include "debug.h"
#include "inbuf.h"
#include "lexer.h"
#include <time.h>

#define FILE_POOL_BLOCK_SIZE	16
#define MACRO_POOL_BLOCK_SIZE	32
#define TOKEN_DATA_BLOCK_SIZE	1024


static const struct {
	char *name;
	enum cpp_directive directive;
}
directives[] = {
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


/*
 * Setup CPP directives. See 6.10 Preprocessing directives.
 */
static void cpp_setup_symtab_directives(struct symtab *table)
{
	struct symbol *symbol;
	struct symdef *def;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(directives); i++) {
		symbol = symtab_insert(table, directives[i].name);

		def = symbol_define(table, symbol);
		def->type = SYMBOL_TYPE_CPP_DIRECTIVE;
		def->directive = directives[i].directive;
	}
}


static void cpp_setup_builtin_handled(struct cpp *cpp, char *name, macro_handler_t *handler)
{
	struct symbol *symbol;
	struct symdef *def;

	symbol = symtab_search_or_insert(&cpp->ctx->symtab, name);

	def = symbol_define(&cpp->ctx->symtab, symbol);
	def->type = SYMBOL_TYPE_CPP_MACRO;

	macro_init(&def->macro);
	def->macro.flags = MACRO_FLAGS_BUILTIN | MACRO_FLAGS_HANDLED;
	def->macro.handler = handler;
}


/*
 * Setup CPP built-in macro.
 */
static void cpp_setup_builtin(struct cpp *cpp, char *name, char *fmt, ...)
{
	struct symbol *symbol;
	struct symdef *def;
	struct strbuf str;
	va_list args;

	symbol = symtab_search_or_insert(&cpp->ctx->symtab, name);

	strbuf_init(&str, 16);
	va_start(args, fmt);
	strbuf_vprintf_at(&str, 0, fmt, args);
	va_end(args);
	
	def = symbol_define(&cpp->ctx->symtab, symbol);
	def->type = SYMBOL_TYPE_CPP_MACRO;

	macro_init(&def->macro);
	def->macro.flags = MACRO_FLAGS_BUILTIN;
	toklist_load_from_strbuf(&def->macro.expansion, cpp->ctx, &str);

	strbuf_free(&str);
}


/* TODO */
static void cpp_builtin_file(struct cpp *cpp, struct macro *macro, struct toklist *out)
{
	(void) macro;
	toklist_load_from_string(out, cpp->ctx, "\"%s\"", "some-file.c");
}

/* TODO */
static void cpp_builtin_line(struct cpp *cpp, struct macro *macro, struct toklist *out)
{
	(void) macro;
	toklist_load_from_string(out, cpp->ctx, "%lu", 128);
}


/*
 * Setup CPP built-ins. See 6.10.8 Predefined macros.
 */
static void cpp_setup_symtab_builtins(struct cpp *cpp)
{
	time_t rawtime;
	struct tm *timeinfo;
	char timestr[32];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(timestr, sizeof(timestr), "\"%T\"", timeinfo);
	cpp_setup_builtin(cpp, "__TIME__", timestr);

	strftime(timestr, sizeof(timestr), "\"%b %e %Y\"", timeinfo); /* TODO shall be non-localized */
	cpp_setup_builtin(cpp, "__DATE__", timestr);

	cpp_setup_builtin(cpp, "__STDC__", "1");
	cpp_setup_builtin(cpp, "__STDC_VERSION__", "201102L"); /* TODO check */
	cpp_setup_builtin(cpp, "__STDC_HOSTED__", "0");

	cpp_setup_builtin_handled(cpp, "__FILE__", cpp_builtin_file);
	cpp_setup_builtin_handled(cpp, "__LINE__", cpp_builtin_line);
}


static void cpp_setup_symtab(struct cpp *cpp)
{
	cpp_setup_symtab_directives(&cpp->ctx->symtab);
	cpp_setup_symtab_builtins(cpp);
}


void cpp_next_token(struct cpp *cpp)
{
	if (!toklist_is_empty(&cpp->tokens))
		cpp->token = toklist_remove_first(&cpp->tokens);
	else
		cpp->token = lexer_next(&cpp_cur_file(cpp)->lexer);

	assert(cpp->token != NULL); /* invariant: EOF guards the list */
}


struct cpp *cpp_new(struct context *ctx)
{
	struct cpp *cpp;

	cpp = mcc_malloc(sizeof(*cpp));

	cpp->ctx = ctx;
	cpp->symtab = &cpp->ctx->symtab; /* TODO */
	cpp->token = NULL;

	objpool_init(&cpp->macro_pool, sizeof(struct macro), MACRO_POOL_BLOCK_SIZE);
	objpool_init(&cpp->file_pool, sizeof(struct cpp_file), FILE_POOL_BLOCK_SIZE);

	list_init(&cpp->file_stack);
	toklist_init(&cpp->tokens);

	cpp_init_ifstack(cpp);
	cpp_setup_symtab(cpp);

	return cpp;
}


void cpp_delete(struct cpp *cpp)
{
	objpool_free(&cpp->macro_pool);
	objpool_free(&cpp->file_pool);
	list_free(&cpp->file_stack);
	toklist_free(&cpp->tokens);

	free(cpp);
}


static void cpp_error_internal(struct cpp *cpp, enum error_level level,
	char *fmt, va_list args)
{
	struct strbuf msg;
	struct cpp_file *file;

	strbuf_init(&msg, 64);
	file = cpp_cur_file(cpp);

	strbuf_vprintf_at(&msg, 0, fmt, args);
	errlist_insert(&cpp->ctx->errlist,
		level,
		"some-file.c",
		strbuf_get_string(&msg),
		strbuf_get_string(&file->lexer.linebuf),
		strbuf_strlen(&file->lexer.linebuf) + 1, // TODO
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


void cpp_warn(struct cpp *cpp, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	cpp_error_internal(cpp, ERROR_LEVEL_WARNING, fmt, args);
	va_end(args);
}


void cpp_error(struct cpp *cpp, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	cpp_error_internal(cpp, ERROR_LEVEL_ERROR, fmt, args);
	va_end(args);
}


static void cpp_requeue_current(struct cpp *cpp)
{
	toklist_insert_first(&cpp->tokens, cpp->token);
}


struct token *cpp_peek(struct cpp *cpp)
{
	struct token *tmp;
	struct token *peek;

	tmp = cpp->token;
	cpp_next_token(cpp);
	peek = cpp->token;
	cpp_requeue_current(cpp);
	cpp->token = tmp;

	return peek;
}


static void cpp_parse_macro_invocation(struct cpp *cpp)
{
	assert(token_is_macro(cpp->token));

	struct macro *macro;
	struct toklist invocation;
	struct toklist expansion;
	unsigned parens_balance = 0;
	bool args_ended = false;

	macro = &cpp->token->symbol->def->macro;
	toklist_init(&invocation);
	toklist_init(&expansion);

	toklist_insert_last(&invocation, cpp->token); /* macro name */
	cpp_next_token(cpp);

	/*
	 * For invocations of function-like macros, this code block will grab
	 * the whole arglist quickly. If the arglist is malformed, this will
	 * be later reported by the macro module, so no errors are handled here.
	 * This is to avoid duplication of the arglist parsing logic contained
	 * in the macro module.
	 */
	if (macro->flags & MACRO_FLAGS_FUNCLIKE && token_is(cpp->token, TOKEN_LPAREN)) {
		while (!token_is_eof(cpp->token)) {
			if (token_is(cpp->token, TOKEN_LPAREN)) {
				parens_balance++;
			}
			else if (token_is(cpp->token, TOKEN_RPAREN)) {
				if (--parens_balance == 0)
					args_ended = true;
			}

			toklist_insert_last(&invocation, cpp->token);
			cpp_next_token(cpp);

			if (args_ended)
				break;
		}
	}

	macro_expand(cpp, &invocation, &expansion);
	cpp_requeue_current(cpp);
	toklist_prepend(&cpp->tokens, &expansion);
	cpp_next_token(cpp);
}


static inline bool cpp_got_hash(struct cpp *cpp)
{
	return token_is(cpp->token, TOKEN_HASH) && cpp->token->is_at_bol;
}


static void cpp_parse(struct cpp *cpp);
struct token *cpp_next(struct cpp *cpp);


static struct token *cpp_cat_literals(struct cpp *cpp, struct toklist *literals)
{
	struct token *strtoken;
	struct strbuf str;
	struct token *first;
	struct token *last;
	size_t len_total = 0;

	strbuf_init(&str, 128);

	toklist_foreach(literal, literals) {
		strbuf_printf(&str, "%s", literal->lstr.str);
		len_total += literal->lstr.len;
	}

	first = toklist_first(literals);
	last = toklist_last(literals);

	strtoken = objpool_alloc(&cpp->ctx->token_pool);
	strtoken->type = TOKEN_STRING_LITERAL;
	strtoken->lstr.str = strbuf_copy_to_mempool(&str, &cpp->ctx->token_data);
	strtoken->lstr.len = strbuf_strlen(&str);
	strtoken->startloc = first->startloc;
	strtoken->endloc = last->endloc;
	strtoken->is_at_bol = first->is_at_bol;
	strtoken->after_white = first->after_white;

	strbuf_free(&str);

	return strtoken;
}


static void cpp_parse(struct cpp *cpp)
{
	while (!token_is_eof(cpp->token)) {
		if (cpp_got_hash(cpp)) {
			cpp_cur_file(cpp)->lexer.emit_eols = true;
			cpp_next_token(cpp);
			cpp_parse_directive(cpp);

		}
		else if (token_is_macro(cpp->token) && !cpp->token->noexpand) {
			/* TODO no cpp_next_token(cpp) here */
			if (macro_is_funclike(&cpp->token->symbol->def->macro)) {
				struct token *macro_name = cpp->token;
				cpp_next_token(cpp);

				if (token_is(cpp->token, TOKEN_LPAREN)) {
					toklist_insert_first(&cpp->tokens, cpp->token);
					cpp->token = macro_name;
					cpp_parse_macro_invocation(cpp);
				}
				else {
					macro_name->noexpand = true;
					cpp->token = macro_name;
					toklist_insert_first(&cpp->tokens, cpp->token);
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
	struct toklist stringles;

	toklist_init(&stringles);

again:
	cpp_parse(cpp);
	tmp = cpp->token;

	if (token_is(cpp->token, TOKEN_STRING_LITERAL)) {
		cpp_next_token(cpp);
		toklist_insert_last(&stringles, tmp);
		goto again;
	}
	else if (!toklist_is_empty(&stringles)) {
		cpp_requeue_current(cpp);
		tmp = cpp_cat_literals(cpp, &stringles);
	}
	else if (token_is_eof(cpp->token)) {
		if (list_length(&cpp->file_stack) == 1)
			return cpp->token;

		cpp_close_file(cpp);
		cpp_next_token(cpp);
		goto again;
	}

	cpp_next_token(cpp);
	return tmp;
}
