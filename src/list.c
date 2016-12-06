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


void *list_find_predecessor(struct list *lst, struct list_node *node)
{
	struct list_node *cur;

	for (cur = &lst->head; cur != NULL; cur = list_next(cur)) {
		if (cur->next == node)
			break;
	}

	return cur;
}


void *list_remove(struct list *lst, struct list_node *node)
{
	struct list_node *pred;

	pred = list_find_predecessor(lst, node);
	assert(pred != NULL);

	pred->next = node->next;
	node->next = NULL;

	return node;
}


struct list list_remove_range(struct list *lst, struct list_node *start, struct list_node *end)
{
	struct list rem_list;
	struct list_node *pred;

	pred = list_find_predecessor(lst, start);
	assert(pred != NULL);

	list_init(&rem_list);

	rem_list.head.next = start;
	rem_list.last = end;
	pred->next = end->next;
	end->next = NULL;

	return rem_list;
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

	assert(first_prepend != list_first(lst)); /* avoid obvious cycle */

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


void list_append(struct list *lst, struct list *lst_to_append)
{
	assert(!list_is_empty(lst_to_append));

	lst->last->next = list_first(lst_to_append);
	lst->last = list_last(lst_to_append);
}


void list_dump(struct list *lst)
{
	printf("List, num_nodes=%lu\n", list_length(lst));
}
