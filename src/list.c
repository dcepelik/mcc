#include "list.h"
#include <assert.h>
#include <stdbool.h>


void list_init(struct list *lst)
{
	lst->head.next = NULL;
	lst->last = &lst->head;
	lst->len = 0;
}


bool list_is_empty(struct list *lst)
{
	return lst->head.next != NULL;
}


void list_insert_last(struct list *lst, struct list_node *node)
{
	node->next = NULL;
	lst->last->next = node;
	lst->last = node;
	lst->len++;
}


void list_insert_first(struct list *lst, struct list_node *node)
{
	if (list_is_empty(lst))
		lst->last = node;

	node->next = lst->head.next;
	lst->head.next = node;
}


void *list_remove_first(struct list *lst)
{
	struct list_node *first = list_first(lst);
	
	if (first) {
		lst->head.next = first->next;

		if (first == lst->last)
			lst->last = &lst->head;

		lst->len--;
	}

	assert(lst->len >= 0);
	
	return first;
}


void *list_first(struct list *lst)
{
	return lst->head.next;
}


void *list_last(struct list *lst)
{
	return lst->last;
}


void *list_next(struct list_node *node)
{
	return node->next;
}


size_t list_length(struct list *lst)
{
	return lst->len;
}


void list_free(struct list *lst)
{
}
