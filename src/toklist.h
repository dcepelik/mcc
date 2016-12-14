/*
 * Token list encapsulation. Trivial code that makes it easier to fight
 * token streams.
 */

#ifndef TOKLIST_H
#define TOKLIST_H

#include "list.h"
#include "token.h"
#include <stdbool.h>

struct toklist
{
	struct context *ctx;
	struct list tokens;
	struct token *current;
};

void toklist_init(struct toklist *toklist);
void toklist_free(struct toklist *toklist);

void toklist_reset(struct toklist *toklist);

struct token *toklist_next(struct toklist *toklist);
struct token *toklist_next_free(struct toklist *toklist);
struct token *toklist_prev(struct toklist *toklist);

bool toklist_is_empty(struct toklist *toklist);
bool toklist_has_more(struct toklist *toklist);

bool toklist_have(struct toklist *toklist, enum token_type token_type);
bool toklist_have_name(struct toklist *toklist);
bool toklist_have_macro(struct toklist *toklist);
bool toklist_have_eof(struct toklist *toklist);
bool toklist_have_eol(struct toklist *toklist);
bool toklist_have_eol_or_eof(struct toklist *toklist);

void toklist_skip(struct toklist *toklist);
void toklist_skip_till(struct toklist *toklist, enum token_type token_type);
void toklist_skip_till_eol(struct toklist *toklist);
void toklist_skip_find(struct toklist *toklist, enum token_type token_type);

#endif
