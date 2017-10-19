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

static void cpp_requeue_current(struct cpp *cpp)
{
	toklist_insert_first(&cpp_this_file(cpp)->tokens, cpp->token);
}

static void cpp_setup_symtab(struct cpp *cpp)
{
	cpp_setup_symtab_directives(&cpp->ctx->symtab);
	cpp_setup_symtab_builtins(cpp);
}

void cpp_next_token(struct cpp *cpp)
{
	struct token *t;
	if (toklist_is_empty(&cpp_this_file(cpp)->tokens)) {
		t = objpool_alloc(&cpp->ctx->token_pool);
		lexer_next(&cpp_this_file(cpp)->lexer, t);
		/* TODO make this per-file, too */
		toklist_insert_first(&cpp_this_file(cpp)->tokens, t);
	}

	cpp->token = toklist_remove_first(&cpp_this_file(cpp)->tokens);
	assert(cpp->token != NULL); /* NOTE: EOF guards the list */
}

static void cpp_error_internal(struct cpp *cpp, enum error_level level, char *fmt, va_list args)
{
	struct strbuf msg;
	struct cpp_file *file;

	strbuf_init(&msg, 64);
	file = cpp_this_file(cpp);

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

struct token *cpp_peek(struct cpp *cpp)
{
	struct token *tmp;
	struct token *peek;

	tmp = cpp->token;
	cpp_next_token(cpp); /* TODO */
	peek = cpp->token;
	cpp_requeue_current(cpp);
	cpp->token = tmp;

	return peek;
}

/*
 * Expect a token on the input. If it's not there, issue an error.
 */
bool cpp_expect(struct cpp *cpp, enum token_type token)
{
	if (cpp->token->type == token)
		return true;

	cpp_error(cpp, "%s was expected, got %s",
		token_get_name(token), token_get_name(cpp->token->type));
	return false;
}

/*
 * Parse a macro invocation.
 *
 * TODO Can I get rid of this?
 */
static void expand_macro_invocation(struct cpp *cpp)
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

	toklist_insert(&invocation, cpp->token); /* macro name */
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

			toklist_insert(&invocation, cpp->token);
			cpp_next_token(cpp);

			if (args_ended)
				break;
		}
	}

	macro_expand(cpp, &invocation, &expansion);
	cpp_requeue_current(cpp);
	toklist_prepend(&cpp_this_file(cpp)->tokens, &expansion);
	cpp_next_token(cpp);
}

/*
 * Given a list of string literals (TOKEN_STRING_LITERAL), concatenate them all
 * to get a new string literal.
 *
 * This is as easy as concatenating the strings and producing a new token,
 * plus setting the appropriate flags on the new token.
 */
static struct token *concat_strings(struct cpp *cpp, struct toklist *literals)
{
	struct token *cat;	/* the concatenation (new token) */
	struct strbuf str;
	struct token *first;
	struct token *last;

	toklist_foreach(literal, literals)
		assert(token_is(literal, TOKEN_STRING_LITERAL));

	strbuf_init(&str, 128);
	toklist_foreach(literal, literals)
		strbuf_printf(&str, "%s", literal->lstr.str);

	first = toklist_first(literals);
	last = toklist_last(literals);

	cat = objpool_alloc(&cpp->ctx->token_pool);
	cat->type = TOKEN_STRING_LITERAL;
	cat->lstr.str = strbuf_copy_to_mempool(&str, &cpp->ctx->token_data);
	cat->lstr.len = strbuf_strlen(&str);
	cat->startloc = first->startloc;
	cat->endloc = last->endloc;
	cat->is_at_bol = first->is_at_bol;
	cat->after_white = first->after_white;

	strbuf_free(&str);
	return cat;
}

/*
 * This function gets the next token and decides what to do with it. If it's
 * the start of a preprocessor directive, it processes it; if it's a macro
 * invocation, it expands it. In an inactive conditional branch, it skips all
 * tokens which aren't CPP directives.
 *
 * It keeps doing this till it runs into a ``fully expanded and preprocessed''
 * token, in which case it stops and `cpp->token' is the next token in the
 * sequence of preprocessed tokens.
 *
 * This function does not carry out anything on its own, it's a dispatch
 * routine.
 */
static void run(struct cpp *cpp)
{
	struct macro *macro;

	while (!token_is_eof(cpp->token)) {
		if (token_is(cpp->token, TOKEN_HASH) && cpp->token->is_at_bol) {
			cpp_process_directive(cpp);
		}
		else if (token_is_macro(cpp->token) && !cpp->token->noexpand) {
			macro = &cpp->token->symbol->def->macro;
			if (!macro_is_funclike(macro) || token_is(cpp_peek(cpp), TOKEN_LPAREN))
				expand_macro_invocation(cpp);
			else
				cpp->token->noexpand = true;
		}
		else if (cpp_is_skip_mode(cpp)) {
			cpp_next_token(cpp);
		}
		else {
			break;
		}
	}
}

/******************************** public API ********************************/

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

	cpp_init_ifstack(cpp);
	cpp_setup_symtab(cpp);

	return cpp;
}

void cpp_delete(struct cpp *cpp)
{
	objpool_free(&cpp->macro_pool);
	objpool_free(&cpp->file_pool);
	list_free(&cpp->file_stack);

	free(cpp);
}

/*
 * This function calls `run' to carry out the actual preprocessing
 * and macro expansion and further filters its output by concatenating
 * adjacent string literals and handling EOF tokens.
 *
 * NOTE: Once TOKEN_EOF is returned for the first time, any further call
 *       to `cpp_next' will return TOKEN_EOF as well, i.e. the TOKEN_EOF
 *       token is inedible. This way, one may always depend on TOKEN_EOF
 *       marking the end of the token stream.
 */
struct token *cpp_next(struct cpp *cpp)
{
	assert(!list_empty(&cpp->file_stack));

	struct toklist stringles;
	struct token *tmp;

	toklist_init(&stringles);

	while (1) {
		run(cpp);
		 /* TODO refactoring aid, remove */
		if (token_is_eol(cpp->token)) {
		}
		else if (token_is(cpp->token, TOKEN_STRING_LITERAL)) {
			toklist_insert(&stringles, cpp->token);
		} else if (!toklist_is_empty(&stringles)) {
			return concat_strings(cpp, &stringles);
		} else if (token_is_eof(cpp->token)) {
			if (list_len(&cpp->file_stack) == 1)
				return cpp->token;
			cpp_close_file(cpp);
		} else {
			tmp = cpp->token;
			cpp_next_token(cpp);
			return tmp;
		}

		cpp_next_token(cpp);
	}
}
