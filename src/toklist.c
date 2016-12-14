#include "toklist.h"


void toklist_init(struct toklist *toklist, struct context *ctx)
{
	toklist->ctx = ctx;
	toklist->current = NULL;

	list_init(&toklist->tokens);
}


void toklist_free(struct toklist *toklist)
{
	list_free(&toklist_tokens);
}


void toklist_reset(struct toklist *toklist)
{
	list_init(&toklist->tokens);
}


struct token *toklist_next(struct toklist *toklist)
{
	return toklist->current = list_next(&toklist->current->list_node);
}


struct token *toklist_next_free(struct toklist *toklist)
{
	/* TODO free */
	return toklist_next(toklist);
}


struct token *toklist_prev(struct toklist *toklist)
{
	struct token *prev;

	prev = list_find_predecessor(&toklist->tokens, &toklist->current->list_node);
	assert(prev != NULL);

	return prev;
}


bool toklist_is_empty(struct toklist *toklist)
{
	return list_is_empty(&toklist->tokens);
}


bool toklist_have(struct toklist *toklist, enum token_type token_type)
{
	return toklist->current && toklist->current->type == token_type;
}


bool toklist_has_more(struct toklist *toklist)
{
	return toklist->current != list_last(&toklist->tokens);
}


bool toklist_have_name(struct toklist *toklist)
{
	return toklist_have(toklist, TOKEN_NAME);
}


bool toklist_have_macro(struct toklist *toklist)
{
	return toklist_have_name(toklist)
		&& toklist->current->symbol->def->type == SYMBOL_TYPE_CPP_MACRO;
}


bool toklist_have_eof(struct toklist *toklist)
{
	return toklist_have(toklist, TOKEN_EOF);
}


bool toklist_have_eol(struct toklist *toklist)
{
	return toklist_have(toklist, TOKEN_EOL);
}


bool toklist_have_eol_or_eof(struct toklist *toklist)
{
	return toklist_have_eol(toklist) || toklist_have_eof(toklist);
}


void toklist_skip(struct toklist *toklist)
{
	toklist_next(toklist);
}


void toklist_skip_till(struct toklist *toklist, enum token_type token_type)
{
	struct token *start = toklist->current;

	while (!toklist_have(toklist, token_type))
		toklist_skip(toklist);

	if (!toklist_have(toklist, token_type))
		toklist->current = start;
}


void toklist_skip_find(struct toklist *toklist, enum token_type token_type)
{
	struct token *start = toklist->current;

	toklist_skip_till(toklist, token_type);
	if (toklist_have(toklist, token_type))
		toklist_skip(toklist);
	else
		toklist->current = start;
}


void toklist_skip_till_eol(struct toklist *toklist)
{
	toklist_skip_till(toklist, TOKEN_EOL);
}
