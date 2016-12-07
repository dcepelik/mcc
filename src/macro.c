#include "cpp.h"
#include "debug.h"
#include "macro.h"
#include "strbuf.h"
#include "symbol.h"
#include <assert.h>
#include <stdio.h>


void macro_init(struct macro *macro)
{
	list_init(&macro->args);
	list_init(&macro->expansion);
	macro->type = MACRO_TYPE_OBJLIKE;
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

	next = list_next(&token->list_node);
	funclike = (next && next->type == TOKEN_LPAREN && !next->preceded_by_whitespace);
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
		dst_token = objpool_alloc(&cpp->token_pool);
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

	token = list_next(&token->list_node); /* macro name */
	token = list_next(&token->list_node); /* ( */

	list_foreach(struct token, arg, &macro->args, list_node) {
		argdef = symbol_define(cpp->symtab, arg->symbol);
		argdef->type = SYMBOL_TYPE_CPP_MACRO;
		argdef->macro = objpool_alloc(&cpp->macro_pool);
		macro_init(argdef->macro);

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
				token = list_next(&token->list_node);
				break;
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

	while ((token = list_first(in)) != NULL) {
		if (!token_is_expandable(token)) {
			list_insert_last(out, list_remove_first(in));
			continue;
		}

		list_init(&expansion);
		end = macro_expand_internal(cpp, in, &expansion);
		list_remove_range(in, &token->list_node, &end->list_node);
		list_append(out, &expansion);
	}
}


struct token *macro_expand_internal(struct cpp *cpp, struct list *in, struct list *out)
{
	struct macro *macro;
	struct token *token;
	struct token *end;	/* end of macro invocation */
	struct list expansion;

	token = list_first(in);
	macro = token->symbol->def->macro;

	symtab_scope_begin(cpp->symtab);

	if (macro->type == MACRO_TYPE_FUNCLIKE)
		end = macro_parse_args(cpp, macro, in);
	else
		end = token;

	list_init(&expansion);
	copy_token_list(cpp, &macro->expansion, &expansion);
	macro_expand_recursive(cpp, &expansion, out);

	symtab_scope_end(cpp->symtab);

	return end;
}


void macro_expand(struct cpp *cpp, struct list *invocation,
	struct list *expansion)
{
	macro_expand_internal(cpp, invocation, expansion);
}
