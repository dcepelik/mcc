/*
 * TODO add #pragma and #line support
 * TODO cexpr support in if conditionals
 */

#include "cpp-internal.h"
#include "context.h"
#include "debug.h"
#include "inbuf.h"
#include "lexer.h"

#define FILE_POOL_BLOCK_SIZE	16
#define MACRO_POOL_BLOCK_SIZE	32
#define TOKEN_DATA_BLOCK_SIZE	1024


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

	cpp = malloc(sizeof(*cpp));
	if (!cpp)
		return NULL;

	cpp->ctx = ctx;
	cpp->symtab = &cpp->ctx->symtab; /* TODO */
	cpp->token = NULL;

	objpool_init(&cpp->macro_pool, sizeof(struct macro), MACRO_POOL_BLOCK_SIZE);
	objpool_init(&cpp->file_pool, sizeof(struct cpp_file), FILE_POOL_BLOCK_SIZE);

	list_init(&cpp->file_stack);
	toklist_init(&cpp->tokens);

	cpp_init_ifstack(cpp);

	cpp_setup_symtab(&cpp->ctx->symtab);

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

	macro = cpp->token->symbol->def->macro;
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
	if (macro->type == MACRO_TYPE_FUNCLIKE && token_is(cpp->token, TOKEN_LPAREN)) {
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


static struct token *cpp_cat_stringles(struct cpp *cpp, struct toklist *stringles)
{
	struct token *strtoken;
	struct strbuf str;
	struct token *first;
	struct token *last;

	strbuf_init(&str, 128);

	toklist_foreach(stringle, stringles)
		strbuf_printf(&str, "%s", stringle->str);

	first = toklist_first(stringles);
	last = toklist_last(stringles);

	strtoken = objpool_alloc(&cpp->ctx->token_pool);
	strtoken->type = TOKEN_STRING;
	strtoken->str = strbuf_copy_to_mempool(&str, &cpp->ctx->token_data);
	strtoken->startloc = first->startloc;
	strtoken->endloc = last->endloc;
	strtoken->is_at_bol = first->is_at_bol;
	strtoken->preceded_by_whitespace = first->preceded_by_whitespace;

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
			if (macro_is_funclike(cpp->token->symbol->def->macro)) {
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

	if (token_is(cpp->token, TOKEN_STRING)) {
		cpp_next_token(cpp);
		toklist_insert_last(&stringles, tmp);
		goto again;
	}
	else if (!toklist_is_empty(&stringles)) {
		cpp_requeue_current(cpp);
		tmp = cpp_cat_stringles(cpp, &stringles);
	}
	else if (token_is_eof(cpp->token)) {
		if (list_length(&cpp->file_stack) == 1)
			return cpp->token;

		cpp_close_file(cpp);
		cpp_next_token(cpp);
		goto again;
	}
	else {
		DEBUG_MSG("Token is:");
		token_dump(cpp->token, stderr);
	}


	cpp_next_token(cpp);
	return tmp;
}
