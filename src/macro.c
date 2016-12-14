#include "context.h"
#include "cpp.h"
#include "debug.h"
#include "macro.h"
#include "objpool.h"
#include "print.h"
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

	DEBUG_MSG("At least a macro");

	next = list_next(&token->list_node);
	funclike = (next && next->type == TOKEN_LPAREN);

	printf(funclike ? "inv funclike\n" : "inv objlike\n");
	printf(macro->type == MACRO_TYPE_FUNCLIKE ? "macro funclike\n" : "macro objlike\n");
	printf("%s\n", symbol_get_name(token->symbol));

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


static struct token *macro_parse_args(struct cpp *cpp,
	struct macro *macro, struct list *invocation)
{
	struct token *token = list_first(invocation);
	struct token *next;
	struct symdef *argdef;
	unsigned parens_balance = 0;
	bool args_ended = false;

	assert(token_is_expandable(token));

	token = list_next(&token->list_node); /* skip macro name */
	token = list_next(&token->list_node); /* skip ( */

	list_foreach(struct token, arg, &macro->args, list_node) {
		argdef = symbol_define(&cpp->ctx->symtab, arg->symbol);
		argdef->type = SYMBOL_TYPE_CPP_MACRO;
		argdef->macro = objpool_alloc(&cpp->macro_pool);

		macro_init(argdef->macro);
		argdef->macro->is_macro_arg = true;

		while (token->type != TOKEN_EOF) {
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
			list_insert_last(&argdef->macro->expansion, &token->list_node);
			token = next;
		}

		if (args_ended) {
			break;
		}
	}

	/* TODO Error checking */
	return token;
}


struct token *macro_expand_internal(struct cpp *cpp, struct list *in, struct list *out);


void macro_expand_recursive(struct cpp *cpp, struct list *in, struct list *out)
{
	struct token *token;
	struct token *end;
	struct list expansion;

	DEBUG_MSG("in toklist:");
	cpp_dump_toklist(in, stderr);

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


static bool token_is_macro_arg(struct token *token)
{
	return token_is_expandable(token)
		&& token->symbol->def->macro->is_macro_arg;
}


static struct token *macro_stringify(struct cpp *cpp, struct list *tokens)
{
	struct strbuf str;
	struct token *strtoken;
	struct token *first;
	struct token *last;

	strbuf_init(&str, 1024);
	list_foreach(struct token, token, tokens, list_node) {
		if (token != list_first(tokens) && token->preceded_by_whitespace)
			strbuf_putc(&str, ' ');

		switch (token->type) {
		case TOKEN_NAME:
			strbuf_printf(&str, symbol_get_name(token->symbol));
			break;

		case TOKEN_STRING:
			print_string_stringify(token->spelling, &str);
			DEBUG_PRINTF("Spelling: %s", token->spelling);
			break;

		case TOKEN_NUMBER:
		case TOKEN_CHAR_CONST:
			strbuf_putc(&str, token->c);
			break;

		default:
			strbuf_printf(&str, (char *)token_get_name(token->type));
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


static void macro_args_prescan(struct cpp *cpp, struct list *in, struct list *out)
{
	struct token *token;
	bool had_hash;
	struct list arg_expansion;

	had_hash = false;
	while ((token = list_first(in)) != NULL) {
		if (token->type == TOKEN_HASH) {
			assert(!had_hash); // TODO 
			had_hash = true;
			list_remove_first(in);
			continue;
		}

		if (had_hash) {
			assert(token_is_expandable(token));

			had_hash = false;
			list_remove_first(in);
			list_insert_last(out, &macro_stringify(cpp, &token->symbol->def->macro->expansion)->list_node);
			continue;
		}

		if (!token_is_macro_arg(token)) {
			list_insert_last(out, list_remove_first(in));
			continue;
		}

		list_init(&arg_expansion);

		macro_expand_internal(cpp, in, &arg_expansion);
		//assert(tmp == list_next(&token->list_node));

		list_remove_first(in);
		list_append(out, &arg_expansion);
	}
}


struct token *macro_expand_internal(struct cpp *cpp, struct list *in, struct list *out)
{
	struct macro *macro;
	struct token *token;
	struct token *end;		/* end of macro invocation */
	struct list expansion;		/* copy of macro expansion list */
	struct list replaced_args;	/* expansion with args fully expanded */

	DEBUG_MSG("in toklist:");
	cpp_dump_toklist(in, stderr);

	if (!list_is_empty(in)) {
		DEBUG_MSG("First token location:");
		location_dump(&((struct token *)list_first(in))->startloc);
		cpp_dump_file_stack(cpp);
	}

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
	macro_args_prescan(cpp, &expansion, &replaced_args);

	macro->is_expanding = true;
	macro_expand_recursive(cpp, &replaced_args, out);
	macro->is_expanding = false;

	symtab_scope_end(&cpp->ctx->symtab);

	return end;
}


void macro_expand(struct cpp *cpp, struct list *invocation,
	struct list *expansion)
{
	DEBUG_TRACE;

	macro_expand_internal(cpp, invocation, expansion);
	DEBUG_MSG("resulting toklist:");
	cpp_dump_toklist(expansion, stderr);
}
