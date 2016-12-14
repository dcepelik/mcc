#ifndef TOKLIST_H
#define TOKLIST_H

#include "context.h"
#include "list.h"
#include "token.h"
#include <stdbool.h>

struct toklist
{
	struct context *ctx;
	struct list *tokens;
	struct token *current;
};

void toklist_init(struct toklist *toklist);
void toklist_free(struct toklist *toklist);

void toklist_reset(struct toklist *toklist);

struct token *toklist_next(struct toklist *toklist);
struct token *toklist_next_free(struct toklist *toklist);
struct token *toklist_prev(struct toklist *toklist);

bool toklist_is_empty(struct toklist *toklist);

#endif
