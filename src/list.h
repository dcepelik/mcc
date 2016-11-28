#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <stdbool.h>

struct list_node
{
	struct list_node *next;
};

struct list
{
	struct list_node head;
	struct list_node *last;
	size_t len;
};

void list_init(struct list *lst);
void list_insert_last(struct list *lst, struct list_node *node);
void list_insert_first(struct list *lst, struct list_node *node);
void *list_remove_first(struct list *lst);
bool list_is_empty(struct list *lst);
void *list_first(struct list *lst);
void *list_next(struct list_node *node);
size_t list_length(struct list *lst);
void list_free(struct list *lst);

#endif
