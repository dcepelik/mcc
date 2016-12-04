#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

#define list_foreach(type, item, lst, member) \
	for (type *item = container_of(list_first(lst), type, member); \
		item != NULL; \
		item = container_of(item->member.next, type, member))
		

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
void list_free(struct list *lst);

void list_prepend(struct list *lst, struct list *lst_to_prepend);

struct list_node *list_insert_first(struct list *lst, struct list_node *node);
struct list_node *list_insert_last(struct list *lst, struct list_node *node);

void *list_remove_first(struct list *lst);
void *list_remove(struct list *lst, struct list_node *node);

void *list_find_predecessor(struct list *lst, struct list_node *node);

bool list_is_empty(struct list *lst);
size_t list_length(struct list *lst);
void *list_first(struct list *lst);
void *list_last(struct list *lst);
void *list_next(struct list_node *node);

#endif
