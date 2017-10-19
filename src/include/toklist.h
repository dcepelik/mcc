/*
 * toklist:
 * Explicit interface for lists of tokens.
 */

#ifndef TOKLIST_H
#define TOKLIST_H

#include "list.h"
#include "strbuf.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct context;

#define toklist_foreach(item, lst) \
	for (struct token *item = container_of(toklist_first(lst), struct token, list_node); \
		item != offsetof(struct token, list_node); \
		item = container_of(item->list_node.next, struct token, list_node))

struct toklist
{
	struct list tokens;
};

void toklist_init(struct toklist *lst);
void toklist_free(struct toklist *lst);

struct token *toklist_first(struct toklist *lst);
struct token *toklist_last(struct toklist *lst);
struct token *toklist_next(struct token *token);

bool toklist_is_empty(struct toklist *lst);
size_t toklist_length(struct toklist *lst);

struct token *toklist_insert_first(struct toklist *lst, struct token *token);
struct token *toklist_insert(struct toklist *lst, struct token *token);

void toklist_prepend(struct toklist *lst, struct toklist *lst_to_prepend);
void toklist_append(struct toklist *lst, struct toklist *lst_to_append);

struct token *toklist_remove_first(struct toklist *lst);
struct token *toklist_remove_last(struct toklist *lst);
struct token *toklist_remove(struct toklist *lst, struct token *token);
void toklist_remove_range(struct toklist *lst, struct token *a, struct token *b);

void toklist_print(struct toklist *tokens, struct strbuf *buf);
void toklist_dump(struct toklist *tokens, FILE *fout);

void toklist_copy(struct context *ctx, struct toklist *src, struct toklist *dst);

void toklist_load_from_strbuf(struct toklist *lst, struct context *ctx, struct strbuf *str);
void toklist_load_from_string(struct toklist *lst, struct context *ctx, char *str, ...);

bool toklist_contains(struct toklist *lst, struct token *token);

#endif
