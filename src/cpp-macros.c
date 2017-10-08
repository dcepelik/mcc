#include "context.h"
#include "cpp-internal.h"
#include "inbuf.h"
#include "lexer.h"
#include "print.h"
#include "strbuf.h"
#include "toklist.h"

void macro_init(struct macro *macro)
{
	macro->name = NULL;
	toklist_init(&macro->args);
	toklist_init(&macro->expansion);
	macro->is_expanding = false;
	macro->flags = 0;
}

void macro_free(struct macro *macro)
{
	toklist_free(&macro->args);
	toklist_free(&macro->expansion);
}

bool macro_is_funclike(struct macro *macro)
{
	return macro->flags & MACRO_FLAGS_FUNCLIKE;
}

static inline bool token_is_expandable_macro(struct token *token)
{
	struct macro *macro;
	struct token *peek;

	if (!token_is_macro(token) || token->noexpand)
		return false;

	macro = &token->symbol->def->macro;

	peek = toklist_next(token);
	if (macro_is_funclike(macro) && (!peek || !token_is(peek, TOKEN_LPAREN)))
		return false;

	return true;
}

static void macro_print(struct macro *macro, struct strbuf *buf)
{
	strbuf_printf(buf, "%s", macro->name);

	if (macro_is_funclike(macro)) {
		strbuf_putc(buf, '(');
		toklist_foreach(arg, &macro->args) {
			token_print(arg, buf);
			if (arg != toklist_last(&macro->args))
				strbuf_printf(buf, ", ");
		}
		strbuf_putc(buf, ')');
	}

	strbuf_putc(buf, ' ');

	toklist_foreach(repl, &macro->expansion) {
		token_print(repl, buf);
		strbuf_putc(buf, ' ');
	}
}

/*
 * See 6.10.3.2 The # operator, par. 2.
 * TODO Work correctly with the lstrs.
 */
static struct token *cpp_stringify(struct cpp *cpp, struct token *token)
{
	struct strbuf str;
	struct toklist *repl_list;
	struct token *first;
	struct token *last;
	struct token *result;

	assert(token_is_macro_arg(token));
	repl_list = &token->symbol->def->macro_arg.tokens;

	strbuf_init(&str, 128);
	toklist_foreach(t, repl_list) {
		if (t->after_white && t != toklist_first(repl_list))
			strbuf_putc(&str, ' ');

		switch (t->type) {
		case TOKEN_CHAR_CONST:
		case TOKEN_STRING_LITERAL:
			print_string_stringify(token_get_spelling(t), &str);
			break;

		default:
			strbuf_printf(&str, token_get_spelling(t));
		}
	}

	result = objpool_alloc(&cpp->ctx->token_pool);
	result->type = TOKEN_STRING_LITERAL;
	result->lstr.str = strbuf_copy_to_mempool(&str, &cpp->ctx->token_data);
	result->lstr.len = strbuf_strlen(&str);
	result->noexpand = false;

	first = toklist_first(repl_list);
	last = toklist_last(repl_list);

	if (first && last) {
		result->startloc = first->startloc;
		result->endloc = last->endloc;
		result->is_at_bol = first->is_at_bol;
		result->after_white = first->after_white;
	}

	strbuf_free(&str);

	return result;
}

static struct token *macro_expand_internal(struct cpp *cpp, struct toklist *in, struct toklist *out);
static void macro_expand_rescan(struct cpp *cpp, struct toklist *in, struct toklist *out);

/*
 * Identify and recursively expand arguments of macro invocation.
 */
static struct token *macro_parse_args(struct cpp *cpp, struct macro *macro, struct toklist *invocation)
{
	struct token *token = toklist_first(invocation);
	struct token *next;
	struct symdef *def;
	struct toklist args;
	int parens_balance = 0;
	bool args_ended = false;

	assert(token_is_expandable_macro(token));
	token = toklist_next(token);

	assert(token->type == TOKEN_LPAREN);
	token = toklist_next(token);

	toklist_foreach(param, &macro->args) {
		toklist_init(&args);

		def = symbol_define(&cpp->ctx->symtab, param->symbol);
		def->type = SYMBOL_TYPE_UNDEF;
		toklist_init(&def->macro_arg.tokens);

		while (token) {
			if (token->type == TOKEN_LPAREN) {
				parens_balance++;
			}
			else if (token->type == TOKEN_RPAREN) {
				if (parens_balance > 0) {
					parens_balance--;
				}
				else {
					args_ended = true;
					break;
				}
			}
			else if (token->type == TOKEN_COMMA && parens_balance == 0) {
				if (param->type != TOKEN_ELLIPSIS) {
					token = toklist_next(token); /* skip , */
					break;
				}
			}

			assert(parens_balance >= 0);

			next = toklist_next(token);
			toklist_remove(invocation, token);
			toklist_insert_last(&args, token);
			token = next;
		}

		/* TODO refactor the rest of this block */

		toklist_copy(cpp->ctx, &args, &def->macro_arg.tokens);

		toklist_init(&def->macro_arg.expansion);
		macro_expand_rescan(cpp, &args, &def->macro_arg.expansion);

		def->type = SYMBOL_TYPE_CPP_MACRO_ARG;

		if (args_ended)
			break;
	}

	/* TODO Error checking */
	return token;
}

static void macro_expand_rescan(struct cpp *cpp, struct toklist *in, struct toklist *out)
{
	struct token *token;
	struct token *end;
	struct toklist expansion;

	while ((token = toklist_first(in)) != NULL) {
		if (!token_is_expandable_macro(token)) {
			toklist_insert_last(out, toklist_remove_first(in));
			continue;
		}

		if (token->symbol->def->macro.is_expanding) {
			token->noexpand = true;
			toklist_insert_last(out, toklist_remove_first(in));
			continue;
		}

		toklist_init(&expansion);
		end = macro_expand_internal(cpp, in, &expansion);
		toklist_remove_range(in, token, end);
		toklist_append(out, &expansion);
	}
}

static struct token *new_placemarker(struct cpp *cpp)
{
	struct token *token;

	token = objpool_alloc(&cpp->ctx->token_pool);
	token->type = TOKEN_PLACEMARKER;

	return token;
}

/*
 * This function ``pastes'' two tokens. It does so by taking the spellings of
 * those two tokens and concatenating them, and then lexes (``re-lexes'') the
 * result. The process should yield a single valid preprocessing token.
 *
 * See TODO.
 */
static struct toklist macro_paste_do(struct cpp *cpp, struct token *a, struct token *b)
{
	struct strbuf buf;
	struct toklist tokens;

	toklist_init(&tokens);

	if (a->type == TOKEN_PLACEMARKER) {
		toklist_insert_first(&tokens, b);
		return tokens;
	}

	if (b->type == TOKEN_PLACEMARKER) {
		toklist_insert_first(&tokens, a);
		return tokens;
	}

	strbuf_init(&buf, 32);
	strbuf_printf(&buf, "%s%s", token_get_spelling(a), token_get_spelling(b));

	toklist_load_from_strbuf(&tokens, cpp->ctx, &buf);
	if (toklist_length(&tokens) != 1)
		DEBUG_PRINTF("pasting %s and %s does not yield single valid preprocessing token",
			token_get_spelling(a), token_get_spelling(b));

	strbuf_free(&buf);

	return tokens;
}

/*
 * If arg is a macro parameter, insert arg's replacement tokens into lst.
 * If the resulting list is empty, insert a placemarker.
 *
 * If arg isn't a macro parameter, just insert it to lst.
 */
void macro_paste_arg_prepare(struct cpp *cpp, struct token *arg, struct toklist *lst)
{
	if (token_is_macro_arg(arg)) {
		toklist_copy(cpp->ctx, &arg->symbol->def->macro_arg.tokens, lst);

		if (toklist_is_empty(lst))
			toklist_insert_last(lst, new_placemarker(cpp));
	}
	else {
		toklist_insert_last(lst, arg);
	}
}

static struct toklist macro_paste(struct cpp *cpp, struct token *a, struct token *b)
{
	struct toklist lst_a;
	struct toklist lst_b;
	struct toklist paste_result;
	struct toklist result;

	toklist_init(&lst_a);
	toklist_init(&lst_b);

	macro_paste_arg_prepare(cpp, a, &lst_a);
	macro_paste_arg_prepare(cpp, b, &lst_b);

	assert(!toklist_is_empty(&lst_a));
	assert(!toklist_is_empty(&lst_b));

	paste_result = macro_paste_do(cpp, toklist_remove_last(&lst_a),
		toklist_remove_first(&lst_b));

	toklist_init(&result);
	toklist_append(&result, &lst_a);
	toklist_append(&result, &paste_result);
	toklist_append(&result, &lst_b);

	return result;
}

static void macro_replace_args(struct cpp *cpp, struct toklist *in, struct toklist *out)
{
	struct token *token;
	struct token *next;
	struct token *arg1;
	struct token *arg2;
	struct toklist paste_result;
	bool hash = false;

	while ((token = toklist_first(in)) != NULL) {
		next = toklist_next(token);

		if (next && token_is(next, TOKEN_HASH_HASH)) {
			toklist_remove(in, next); /* remove ## */
			next = toklist_next(token); /* second arg */

			arg1 = token;
			arg2 = next;

			toklist_remove(in, arg1);
			toklist_remove(in, arg2);

			assert(arg2 != NULL);

			paste_result = macro_paste(cpp, arg1, arg2);
			toklist_append(out, &paste_result);
		}
		else if (token_is(token, TOKEN_HASH)) {
			assert(!hash); /* Not # after # TODO Error reporting */
			hash = true;
			toklist_remove_first(in);
		}
		else if (!token_is_macro_arg(token)) {
			assert(!hash); /* Not # {notarg} TODO Error reporting */
			toklist_insert_last(out, toklist_remove_first(in));
		}
		else if (hash) {
			toklist_insert_last(out, cpp_stringify(cpp, token));
			toklist_remove_first(in);
			hash = false;
		}
		else {
			toklist_copy(cpp->ctx, &token->symbol->def->macro_arg.expansion, out);
			toklist_remove_first(in);
		}
	}
}

static struct token *macro_expand_internal(struct cpp *cpp, struct toklist *in, struct toklist *out)
{
	struct macro *macro;
	struct token *token;
	struct token *end;
	struct toklist expansion;
	struct toklist replaced_args;

	token = toklist_first(in);

	assert(token_is_expandable_macro(token));
	macro = &token->symbol->def->macro;

	toklist_init(&expansion);
	toklist_init(&replaced_args);

	assert(!macro->is_expanding);

	if (macro->flags & MACRO_FLAGS_HANDLED) {
		/*
		 * Temporary assert. Right now, we're only using handled
		 * macros to implement CPP built-ins, all of which are
		 * object-like.
		 */
		assert(!macro_is_funclike(macro));

		macro->handler(cpp, macro, out);
		return token;
	}

	symtab_scope_begin(&cpp->ctx->symtab);

	if (macro_is_funclike(macro))
		end = macro_parse_args(cpp, macro, in);
	else
		end = token;

	toklist_copy(cpp->ctx, &macro->expansion, &expansion);
	macro_replace_args(cpp, &expansion, &replaced_args);

	toklist_foreach(token, &replaced_args) {
		if (token->type == TOKEN_PLACEMARKER)
			toklist_remove(&replaced_args, token);
	}

	/*
	 * Notice: symtab scope ended prior to recursive expansion.
	 */
	symtab_scope_end(&cpp->ctx->symtab);

	macro->is_expanding = true;
	macro_expand_rescan(cpp, &replaced_args, out);
	macro->is_expanding = false;

	return end;
}

void macro_expand(struct cpp *cpp, struct toklist *in, struct toklist *out)
{
	macro_expand_internal(cpp, in, out);
}
