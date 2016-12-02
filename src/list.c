#include "list.h"
#include "debug.h"
#include <assert.h>


void list_init(struct list *lst)
{
	lst->head.next = NULL;
	lst->last = &lst->head;
	lst->len = 0;
}


size_t list_length(struct list *lst)
{
	return lst->len;
}


bool list_is_empty(struct list *lst)
{
	return lst->last == &lst->head;
}


struct list_node *list_insert_last(struct list *lst, struct list_node *node)
{
	node->next = NULL;
	lst->last->next = node;
	lst->last = node;
	lst->len++;

	return node;
}


struct list_node *list_insert_first(struct list *lst, struct list_node *node)
{
	if (list_is_empty(lst))
		lst->last = node;

	node->next = lst->head.next;
	lst->head.next = node;
	lst->len++;

	return node;
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


void list_free(struct list *lst)
{
}


void list_prepend(struct list *lst, struct list *lst_to_prepend)
{
	struct list_node *first_prepend;
	struct list_node *last_prepend;

	first_prepend = list_first(lst_to_prepend);
	last_prepend = list_last(lst_to_prepend);

	last_prepend->next = list_first(lst);
	lst->head.next = first_prepend;

	lst->len += list_length(lst_to_prepend);

	if (lst->last == &lst->head)
		lst->last = last_prepend;

	assert(list_is_empty(lst_to_prepend) || !list_is_empty(lst));
	assert(list_length(lst) >= list_length(lst_to_prepend));
}


void list_dump(struct list *lst)
{
	printf("List, num_nodes=%lu\n", list_length(lst));
}
