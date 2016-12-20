#include "context.h"
#include "cpp-internal.h"
#include "inbuf.h"
#include "lexer.h"
#include "print.h"
#include "strbuf.h"
#include "toklist.h"


void macro_init(struct macro *macro)
{
	toklist_init(&macro->args);
	toklist_init(&macro->expansion);
	macro->is_expanding = false;
}


void macro_free(struct macro *macro)
{
	toklist_free(&macro->args);
	toklist_free(&macro->expansion);
}


static inline bool macro_is_funclike(struct macro *macro)
{
	return macro->type == MACRO_TYPE_FUNCLIKE;
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


static inline bool token_is_expandable_macro(struct token *token)
{
	struct macro *macro;
	struct token *peek;

	if (!token_is_macro(token) || token->noexpand)
		return false;

	macro = token->symbol->def->macro;

	peek = toklist_next(token);
	if (macro_is_funclike(macro) && (!peek || !token_is(peek, TOKEN_LPAREN)))
		return false;

	return true;
}


struct toklist paste(struct cpp *cpp, struct token *a, struct token *b)
{
	struct strbuf buf;
	struct inbuf inbuf;
	struct lexer lexer;
	struct toklist tokens;
	struct token *token;
	size_t num_tokens = 0;

	toklist_init(&tokens);
	if (a->type == TOKEN_PLACEMARKER) {
		toklist_insert_first(&tokens, b);
		return tokens;
	}

	if (b->type == TOKEN_PLACEMARKER) {
		toklist_insert_first(&tokens, a);
		return tokens;
	}

	/* TODO Error checking */

	strbuf_init(&buf, 32);
	strbuf_printf(&buf, "%s%s", token_get_spelling(a), token_get_spelling(b));

	DEBUG_PRINTF("strbuf is %s", strbuf_get_string(&buf));

	inbuf_open_mem(&inbuf, strbuf_get_string(&buf), strbuf_strlen(&buf));

	lexer_init(&lexer, cpp->ctx, &inbuf);

	while ((token = lexer_next(&lexer))) {
		if (token->type == TOKEN_EOF)
			break;

		toklist_insert_last(&tokens, token);
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
static struct token *macro_stringify(struct cpp *cpp, struct toklist *tokens)
{
	struct strbuf str;
	struct token *strtoken;
	struct token *first;
	struct token *last;

	strbuf_init(&str, 1024);
	toklist_foreach(token, tokens) {
		if (token->preceded_by_whitespace && token != toklist_first(tokens))
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

	first = toklist_first(tokens);
	last = toklist_last(tokens);

	strtoken = objpool_alloc(&cpp->ctx->token_pool);
	strtoken->type = TOKEN_STRING;
	strtoken->str = strbuf_copy_to_mempool(&str, &cpp->ctx->token_data);
	strtoken->noexpand = false;

	if (first && last) {
		strtoken->startloc = first->startloc;
		strtoken->endloc = last->endloc;
		strtoken->is_at_bol = first->is_at_bol;
		strtoken->preceded_by_whitespace = first->preceded_by_whitespace;
	}

	strbuf_free(&str);

	return strtoken;
}


/*static void copy_token_list(struct cpp *cpp, struct list *src_list, struct list *dst_list)
{
	struct token *dst;

	list_foreach(struct token, src, src_list, list_node) {
		dst = objpool_alloc(&cpp->ctx->token_pool);
		*dst = *src;
		list_insert_last(dst_list, &dst->list_node);
	}

	assert(list_length(dst_list) == list_length(src_list));
}*/


struct token *macro_expand_internal(struct cpp *cpp, struct toklist *in, struct toklist *out);
void macro_expand_recursive(struct cpp *cpp, struct toklist *in, struct toklist *out);


/*
 * Identify and recursively expand arguments of macro invocation.
 */
static struct token *macro_parse_args(struct cpp *cpp, struct macro *macro, struct toklist *invocation)
{
	struct token *token = toklist_first(invocation);
	struct token *next;
	struct symdef *def;
	struct toklist params;
	unsigned parens_balance = 0;
	bool args_ended = false;

	assert(token_is_expandable_macro(token));

	token = toklist_next(token); /* skip macro name */
	token = toklist_next(token); /* skip ( */

	toklist_foreach(arg, &macro->args) {
		toklist_init(&params);

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
					token = toklist_next(token); /* skip , */
					break;
				}
			}

			assert(parens_balance >= 0);

			next = toklist_next(token);
			toklist_remove(invocation, token);
			toklist_insert_last(&params, token);
			token = next;
		}

		/* TODO refactor the rest of this block */

		def = symbol_define(&cpp->ctx->symtab, arg->symbol);
		def->type = SYMBOL_TYPE_UNDEF;

		toklist_init(&def->macro_arg.tokens);
		toklist_copy(cpp->ctx, &params, &def->macro_arg.tokens);
		
		toklist_init(&def->macro_arg.expansion);
		macro_expand_recursive(cpp, &params, &def->macro_arg.expansion);

		def->type = SYMBOL_TYPE_CPP_MACRO_ARG;

		if (args_ended)
			break;
	}

	/* TODO Error checking */
	return token;
}


void macro_expand_recursive(struct cpp *cpp, struct toklist *in, struct toklist *out)
{
	struct token *token;
	struct token *end;
	struct toklist expansion;

	while ((token = toklist_first(in)) != NULL) {
		if (!token_is_expandable_macro(token)) {
			toklist_insert_last(out, toklist_remove_first(in));
			continue;
		}

		if (token->symbol->def->macro->is_expanding) {
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


struct token *new_placemarker(struct cpp *cpp)
{
	struct token *token;

	token = objpool_alloc(&cpp->ctx->token_pool);
	token->type = TOKEN_PLACEMARKER;

	return token;
}


static void macro_replace_args(struct cpp *cpp, struct toklist *in, struct toklist *out)
{
	struct token *token;
	struct token *next;
	struct token *arg1;
	struct token *arg2;
	struct token *paste1;
	struct token *paste2;
	struct toklist arg_expansion;
	struct toklist arg1_expansion;
	struct toklist arg2_expansion;
	bool hash = false;

	while ((token = toklist_first(in)) != NULL) {
		next = toklist_next(token);

		if (next && next->type == TOKEN_HASH_HASH) {
			toklist_remove(in, next); /* remove ## */
			next = toklist_next(token); /* second arg */

			arg1 = token;
			arg2 = next;

			toklist_remove(in, arg1);
			toklist_remove(in, arg2);

			assert(arg2 != NULL);

			toklist_init(&arg1_expansion);
			toklist_init(&arg2_expansion);

			if (token_is_macro_arg(arg1)) {
				if (toklist_is_empty(&arg1->symbol->def->macro_arg.tokens))
					toklist_insert_first(&arg1_expansion, new_placemarker(cpp));
				else 
					toklist_copy(cpp->ctx, &arg1->symbol->def->macro_arg.tokens, &arg1_expansion);
			}
			else {
				toklist_insert_last(&arg1_expansion, arg1);
			}

			if (token_is_macro_arg(arg2)) {
				if (toklist_is_empty(&arg2->symbol->def->macro_arg.tokens))
					toklist_insert_first(&arg2_expansion, new_placemarker(cpp));
				else
					toklist_copy(cpp->ctx, &arg2->symbol->def->macro_arg.tokens, &arg2_expansion);
			}
			else {
				toklist_insert_last(&arg2_expansion, arg2);
			}

			paste1 = toklist_remove_last(&arg1_expansion);
			paste2 = toklist_remove_first(&arg2_expansion);

			struct toklist pasted = paste(cpp, paste1, paste2);

			toklist_append(out, &arg1_expansion);
			toklist_append(out, &pasted);
			toklist_append(out, &arg2_expansion);
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
		else {
			toklist_init(&arg_expansion);
			toklist_copy(cpp->ctx, &token->symbol->def->macro_arg.expansion,
				&arg_expansion);

			if (hash) {
				toklist_insert_last(out, macro_stringify(cpp, &arg_expansion));
				hash = false;
			}
			else {
				toklist_append(out, &arg_expansion);
			}

			toklist_remove_first(in);
		}
	}
}


struct token *macro_expand_internal(struct cpp *cpp, struct toklist *in, struct toklist *out)
{
	struct macro *macro;
	struct token *token;
	struct token *end;		/* end of macro invocation */
	struct toklist expansion;		/* copy of macro expansion list */
	struct toklist replaced_args;	/* expansion with args fully expanded */

	token = toklist_first(in);
	macro = token->symbol->def->macro;

	assert(!macro->is_expanding);

	symtab_scope_begin(&cpp->ctx->symtab);

	if (macro->type == MACRO_TYPE_FUNCLIKE)
		end = macro_parse_args(cpp, macro, in);
	else
		end = token;

	toklist_init(&expansion);
	toklist_copy(cpp->ctx, &macro->expansion, &expansion);

	toklist_init(&replaced_args);
	macro_replace_args(cpp, &expansion, &replaced_args);

	/*
	 * Notice: symtab scope ended prior to recursive expansion.
	 *         Otherwise, this wouldn't work well.
	 */
	symtab_scope_end(&cpp->ctx->symtab);

	macro->is_expanding = true;
	macro_expand_recursive(cpp, &replaced_args, out);
	macro->is_expanding = false;

	return end;
}


void macro_expand(struct cpp *cpp, struct toklist *invocation, struct toklist *expansion)
{
	macro_expand_internal(cpp, invocation, expansion);
}
