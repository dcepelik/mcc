#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

#define list_foreach(type, item, l, member) \
	for (type *item = container_of(list_first(l), type, member); \
		item != offsetof(type, member); \
		item = container_of(item->member.next, type, member))

/*
 * Node of a singly-linked list.
 */
struct lnode
{
	struct lnode *next;	/* next node in the list */
};

void *lnode_next(struct lnode *n);

/*
 * Singly-linked list with a head.
 */
struct list
{
	struct lnode head;	/* the head of the list */
	struct lnode *last;	/* the last node in the list or @head */
};

void list_init(struct list *l);
void list_free(struct list *l);

bool list_empty(struct list *l);
size_t list_len(struct list *l);

void *list_first(struct list *l);
void *list_last(struct list *l);

void *list_find_prev(struct list *l, struct lnode *n);

bool list_contains(struct list *l, struct lnode *n);

/*
 * TODO
 * Concatenate @l1 and @l2. List @l1 will become the result and
 * list @l2 will be empty.
 */

// list_cat(struct list *l1, struct list *l2);
void list_prepend(struct list *l, struct list *lst_to_prepend);
void list_append(struct list *l, struct list *lst_to_append);

void list_insert_list_after(struct list *l, struct lnode *after, struct list *lst_to_insert);

void *list_insert(struct list *l, struct lnode *n);
void *list_insert_head(struct list *l, struct lnode *n);
void list_insert_after(struct list *l, struct lnode *after, struct lnode *n);

void *list_remove(struct list *l);
void *list_remove_head(struct list *l);
void *list_remove_node(struct list *l, struct lnode *n);

/*
 * TODO Remove this as it makes no sense. Most of the time, I want to have
 *      a hand on the items I remove so that I can dispose them properly.
 */
struct list list_remove_range(struct list *l, struct lnode *start, struct lnode *end);

#endif
