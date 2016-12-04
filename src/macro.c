#include "debug.h"
#include "macro.h"
#include "strbuf.h"
#include <assert.h>
#include <stdio.h>


void macro_init(struct macro *macro)
{
	list_init(&macro->args);
	list_init(&macro->expansion);
}


void macro_free(struct macro *macro)
{
	list_free(&macro->args);
	list_free(&macro->expansion);
}


void macro_dump(struct macro *macro)
{
	struct tokinfo *arg;
	struct tokinfo *repl;
	struct strbuf buf;

	strbuf_init(&buf, 128);
	strbuf_printf(&buf, "%s", macro->name);

	if (macro->type == MACRO_TYPE_FUNCLIKE) {
		strbuf_putc(&buf, '(');
		for (arg = list_first(&macro->args); arg; arg = list_next(&arg->list_node)) {
			tokinfo_print(arg, &buf);
			if (arg != list_last(&macro->args))
				strbuf_printf(&buf, ", ");
		}
		strbuf_putc(&buf, ')');
	}

	strbuf_printf(&buf, " -> ");

	for (repl = list_first(&macro->expansion); repl; repl = list_next(&repl->list_node)) {
		tokinfo_print(repl, &buf);
		strbuf_putc(&buf, ' ');
	}

	printf("%s\n", strbuf_get_string(&buf));
	strbuf_free(&buf);
}


static inline bool token_is_expandable(struct tokinfo *token)
{
	struct macro *macro;
	struct tokinfo *next;
	bool funclike;

	if (token->token != TOKEN_NAME
		|| token->symbol->def->type != SYMBOL_TYPE_CPP_MACRO)
		return false;

	macro = token->symbol->def->macro;

	next = list_next(&token->list_node);
	funclike = (next->token == TOKEN_LPAREN && !next->preceded_by_whitespace);
	if (funclike && macro->type == MACRO_TYPE_FUNCLIKE)
		return true;

	if (!funclike && macro->type == MACRO_TYPE_OBJLIKE)
		return true;

	return false;
}


static void copy_token_list(struct cppfile *file, struct list *src, struct list *dst)
{
	struct tokinfo *dst_token;

	list_foreach(struct tokinfo, src_token, src, list_node) {
		dst_token = objpool_alloc(&file->tokinfo_pool);
		*dst_token = *src_token;
		list_insert_last(dst, &dst_token->list_node);
	}

	/* TODO handle errors */

	assert(list_length(dst) == list_length(src));
}


static struct tokinfo *macro_parse_args(struct cppfile *file,
	struct macro *macro, struct list *invocation)
{
	struct tokinfo *token = list_first(invocation);
	struct tokinfo *next;
	struct symdef *argdef;
	unsigned parens_balance = 0;
	bool args_ended = false;

	assert(token_is_expandable(token));

	token = list_next(&token->list_node); /* macro name */
	token = list_next(&token->list_node); /* ( */

	list_foreach(struct tokinfo, arg, &macro->args, list_node) {
		argdef = objpool_alloc(&file->symdef_pool);
		argdef->type = SYMBOL_TYPE_CPP_MACRO;
		argdef->macro = objpool_alloc(&file->macro_pool);
		macro_init(argdef->macro);

		symbol_push_definition(file->symtab, arg->symbol, argdef);

		while (token->token != TOKEN_EOF) {
			if (token->token == TOKEN_LPAREN) {
				parens_balance++;
			}
			else if (token->token == TOKEN_RPAREN) {
				if (parens_balance > 0) {
					parens_balance--;
				}
				else {
					args_ended = true;
					break;
				}
			}
			else if (token->token == TOKEN_COMMA && parens_balance == 0) {
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


static struct list list_range(struct list_node *first, struct list_node *last)
{
	return (struct list) {
		.head = {
			.next = first
		},
		.last = last
	};
}


static void macro_expand_internal(struct cppfile *file, struct list *invocation,
	struct list *expansion);


static void macro_expand_list(struct cppfile *file, struct list *in,
	struct list *out)
{
	struct list invocation;
	struct list tmp;
	struct list expansion;
	struct tokinfo *end;
	struct macro *macro;

	list_foreach(struct tokinfo, token, in, list_node) {
		if (!token_is_expandable(token)) {
			list_insert_last(out, &token->list_node);
			continue;
		}

		macro = token->symbol->def->macro;

		if (macro->type == MACRO_TYPE_FUNCLIKE) {
			tmp = list_range(&token->list_node, list_last(in));
			end = macro_parse_args(file, macro, &tmp);
		}
		else {
			end = token;
		}

		list_init(&expansion);
		invocation = list_range(&token->list_node, &end->list_node);
		macro_expand_internal(file, &invocation, &expansion);
	}
}


static void macro_expand_internal(struct cppfile *file, struct list *invocation,
	struct list *expansion)
{
	struct macro *macro;
	struct tokinfo *start;

	start = list_first(invocation);
	assert(token_is_expandable(start));

	macro = start->symbol->def->macro;

	symtab_scope_begin(file->symtab);

	if (macro->type == MACRO_TYPE_FUNCLIKE)
		macro_parse_args(file, macro, invocation);

	copy_token_list(file, &macro->expansion, expansion);

	struct list out;
	list_init(&out);
	macro_expand_list(file, expansion, &out);
	cpp_dump_toklist(&out);

	symtab_scope_end(file->symtab);
}


void macro_expand(struct cppfile *file, struct list *invocation,
	struct list *expansion)
{
	macro_expand_internal(file, invocation, expansion);
}
