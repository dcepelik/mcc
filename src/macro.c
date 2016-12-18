#include "context.h"
#include "debug.h"
#include "inbuf.h"
#include "lexer.h"
#include "macro.h"
#include "print.h"
#include "strbuf.h"
#include "strbuf.h"
#include "symbol.h"
#include "token.h"
#include <assert.h>
#include <stdio.h>


void macro_init(struct macro *macro)
{
	list_init(&macro->args);
	list_init(&macro->expansion);
	macro->type = MACRO_TYPE_OBJLIKE;
	macro->is_macro_arg = false;
	macro->is_expanding = false;
}


void macro_free(struct macro *macro)
{
	list_free(&macro->args);
	list_free(&macro->expansion);
}


struct list paste(struct cpp *cpp, struct token *a, struct token *b)
{
	struct strbuf buf;
	struct inbuf inbuf;
	struct lexer lexer;
	struct list tokens;
	struct token *token;
	size_t num_tokens = 0;

	/* TODO Error checking */

	strbuf_init(&buf, 32);
	strbuf_printf(&buf, "%s%s", token_get_spelling(a), token_get_spelling(b));

	DEBUG_PRINTF("strbuf is %s", strbuf_get_string(&buf));

	inbuf_open_mem(&inbuf, strbuf_get_string(&buf), strbuf_strlen(&buf));

	lexer_init(&lexer, cpp->ctx, &inbuf);

	list_init(&tokens);

	while ((token = lexer_next(&lexer))) {
		if (token->type == TOKEN_EOF)
			break;

		list_insert_last(&tokens, &token->list_node);
		num_tokens++;
	}

	assert(token->type == TOKEN_EOF);

	if (num_tokens != 1)
		DEBUG_PRINTF("pasting %s and %s does not give a valid preprocessing token",
			token_get_spelling(a), token_get_spelling(b));

	lexer_free(&lexer);
	inbuf_close(&inbuf);
	strbuf_free(&buf);

	return tokens;
}


/*
 * 6.10.3.2-2
 */
static struct token *macro_stringify(struct cpp *cpp, struct list *tokens)
{
	struct strbuf str;
	struct token *strtoken;
	struct token *first;
	struct token *last;

	strbuf_init(&str, 1024);
	list_foreach(struct token, token, tokens, list_node) {
		if (token->preceded_by_whitespace && token != list_first(tokens))
			strbuf_putc(&str, ' ');

		switch (token->type) {
		case TOKEN_CHAR_CONST:
		case TOKEN_STRING:
			print_string_stringify(token_get_spelling(token), &str);
			break;

		default:
			strbuf_printf(&str, token_get_spelling(token));
		}
	}

	first = list_first(tokens);
	last = list_last(tokens);

	strtoken = objpool_alloc(&cpp->ctx->token_pool);
	strtoken->type = TOKEN_STRING;
	strtoken->str = strbuf_copy_to_mempool(&str, &cpp->ctx->token_data);
	strtoken->startloc = first->startloc;
	strtoken->endloc = last->endloc;
	strtoken->preceded_by_whitespace = first->preceded_by_whitespace;
	strtoken->is_at_bol = first->is_at_bol;
	strtoken->noexpand = false;

	strbuf_free(&str);

	return strtoken;
}


void macro_dump(struct macro *macro)
{
	struct token *arg;
	struct token *repl;
	struct strbuf buf;

	strbuf_init(&buf, 128);
	strbuf_printf(&buf, "%s", macro->name);

	if (macro->type == MACRO_TYPE_FUNCLIKE) {
		strbuf_putc(&buf, '(');
		for (arg = list_first(&macro->args); arg; arg = list_next(&arg->list_node)) {
			token_print(arg, &buf);
			if (arg != list_last(&macro->args))
				strbuf_printf(&buf, ", ");
		}
		strbuf_putc(&buf, ')');
	}

	strbuf_printf(&buf, " -> ");

	for (repl = list_first(&macro->expansion); repl; repl = list_next(&repl->list_node)) {
		token_print(repl, &buf);
		strbuf_putc(&buf, ' ');
	}

	printf("%s\n", strbuf_get_string(&buf));
	strbuf_free(&buf);
}


static inline bool token_is_expandable(struct token *token)
{
	struct macro *macro;
	struct token *next;
	bool funclike;

	if (token->type != TOKEN_NAME
		|| token->symbol->def->type != SYMBOL_TYPE_CPP_MACRO)
		return false;

	macro = token->symbol->def->macro;

	next = list_next(&token->list_node);
	funclike = (next && next->type == TOKEN_LPAREN);

	if (funclike && macro->type == MACRO_TYPE_FUNCLIKE)
		return true;

	if (!funclike && macro->type == MACRO_TYPE_OBJLIKE)
		return true;

	return false;
}


static void copy_token_list(struct cpp *cpp, struct list *src, struct list *dst)
{
	struct token *dst_token;

	list_foreach(struct token, src_token, src, list_node) {
		dst_token = objpool_alloc(&cpp->ctx->token_pool);
		*dst_token = *src_token;
		list_insert_last(dst, &dst_token->list_node);
	}

	/* TODO handle errors */

	assert(list_length(dst) == list_length(src));
}


static bool token_is_macro_arg(struct token *token)
{
	return token_is_expandable(token)
		&& token->symbol->def->macro->is_macro_arg;
}


struct token *macro_expand_internal(struct cpp *cpp, struct list *in, struct list *out);
void macro_expand_recursive(struct cpp *cpp, struct list *in, struct list *out);


/*
 * Identify and recursively expand arguments of macro invocation.
 */
static struct token *macro_parse_args(struct cpp *cpp, struct macro *macro, struct list *invocation)
{
	struct token *token = list_first(invocation);
	struct token *next;
	struct symdef *argdef;
	struct list params;
	unsigned parens_balance = 0;
	bool args_ended = false;

	assert(token_is_expandable(token));

	token = list_next(&token->list_node); /* skip macro name */
	token = list_next(&token->list_node); /* skip ( */

	list_foreach(struct token, arg, &macro->args, list_node) {
		list_init(&params);

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
				if (arg->type != TOKEN_ELLIPSIS) {
					token = list_next(&token->list_node); /* skip , */
					break;
				}
			}

			assert(parens_balance >= 0);

			next = list_next(&token->list_node);
			list_remove(invocation, &token->list_node);
			list_insert_last(&params, &token->list_node);
			token = next;
		}

		/* TODO refactor the rest of this block */

		argdef = symbol_define(&cpp->ctx->symtab, arg->symbol);
		argdef->type = SYMBOL_TYPE_UNDEF;
		argdef->macro = objpool_alloc(&cpp->macro_pool);

		macro_init(argdef->macro);

		macro_expand_recursive(cpp, &params, &argdef->macro->expansion);
		argdef->macro->is_macro_arg = true; /* needs to be here */
		argdef->type = SYMBOL_TYPE_CPP_MACRO;

		if (args_ended)
			break;
	}

	/* TODO Error checking */
	return token;
}


void macro_expand_recursive(struct cpp *cpp, struct list *in, struct list *out)
{
	struct token *token;
	struct token *end;
	struct list expansion;

	while ((token = list_first(in)) != NULL) {
		if (!token_is_expandable(token)) {
			list_insert_last(out, list_remove_first(in));
			continue;
		}

		if (token->symbol->def->macro->is_expanding) {
			token->noexpand = true;
			list_insert_last(out, list_remove_first(in));
			continue;
		}

		list_init(&expansion);
		end = macro_expand_internal(cpp, in, &expansion);
		list_remove_range(in, &token->list_node, &end->list_node);
		list_append(out, &expansion);
	}
}


static void macro_replace_args(struct cpp *cpp, struct list *in, struct list *out)
{
	struct token *token;
	struct token *next;
	struct token *arg1;
	struct token *arg2;
	struct token *paste1;
	struct token *paste2;
	struct list arg_expansion;
	struct list arg1_expansion;
	struct list arg2_expansion;
	bool hash = false;

	while ((token = list_first(in)) != NULL) {
		next = list_next(&token->list_node);

		if (next && next->type == TOKEN_HASH_HASH) {
			list_remove(in, &next->list_node); /* remove ## */
			next = list_next(&token->list_node); /* second arg */

			arg1 = token;
			arg2 = next;

			list_remove(in, &arg1->list_node);
			list_remove(in, &arg2->list_node);

			assert(token_is_macro_arg(arg1));
			assert(arg2 != NULL && token_is_macro_arg(arg2));

			list_init(&arg1_expansion);
			list_init(&arg2_expansion);

			copy_token_list(cpp, &arg1->symbol->def->macro->expansion, &arg1_expansion);
			copy_token_list(cpp, &arg2->symbol->def->macro->expansion, &arg2_expansion);

			paste1 = list_remove_last(&arg1_expansion);
			paste2 = list_remove_first(&arg2_expansion);

			struct list pasted = paste(cpp, paste1, paste2);

			list_append(out, &arg1_expansion);
			list_append(out, &pasted);
			list_append(out, &arg2_expansion);
		}
		else if (token_is(token, TOKEN_HASH)) {
			assert(!hash); /* Not # after # TODO Error reporting */
			hash = true;
			list_remove_first(in);
		}
		else if (!token_is_macro_arg(token)) {
			assert(!hash); /* Not # {notarg} TODO Error reporting */
			list_insert_last(out, list_remove_first(in));
		}
		else {
			list_init(&arg_expansion);
			copy_token_list(cpp, &token->symbol->def->macro->expansion,
				&arg_expansion);

			if (hash) {
				list_insert_last(out, &macro_stringify(cpp, &arg_expansion)->list_node);
				hash = false;
			}
			else {
				list_append(out, &arg_expansion);
			}

			list_remove_first(in);
		}
	}
}


struct token *macro_expand_internal(struct cpp *cpp, struct list *in, struct list *out)
{
	struct macro *macro;
	struct token *token;
	struct token *end;		/* end of macro invocation */
	struct list expansion;		/* copy of macro expansion list */
	struct list replaced_args;	/* expansion with args fully expanded */

	token = list_first(in);
	macro = token->symbol->def->macro;

	assert(!macro->is_expanding);

	symtab_scope_begin(&cpp->ctx->symtab);

	if (macro->type == MACRO_TYPE_FUNCLIKE)
		end = macro_parse_args(cpp, macro, in);
	else
		end = token;

	list_init(&expansion);
	copy_token_list(cpp, &macro->expansion, &expansion);

	list_init(&replaced_args);
	macro_replace_args(cpp, &expansion, &replaced_args);

	symtab_scope_end(&cpp->ctx->symtab);

	/* notice: symtab scope ended prior to recursive expansion */

	macro->is_expanding = true;
	macro_expand_recursive(cpp, &replaced_args, out);
	macro->is_expanding = false;

	return end;
}


void macro_expand(struct cpp *cpp, struct list *invocation,
	struct list *expansion)
{
	macro_expand_internal(cpp, invocation, expansion);
}
