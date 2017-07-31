#include "list.h"
#include "debug.h"
#include <assert.h>


void list_init(struct list *lst)
{
	lst->head.next = NULL;
	lst->last = &lst->head;
}


size_t list_length(struct list *lst)
{
	struct list_node *node;
	size_t len = 0;

	for (node = list_first(lst); node != NULL; node = list_next(node))
		len++;

	return len;
}


bool list_is_empty(struct list *lst)
{
	return lst->last == &lst->head;
}


void list_insert_after(struct list *lst, struct list_node *after, struct list_node *node)
{
	node->next = after->next;
	after->next = node;

	if (after == lst->last)
		lst->last = node;
}


void list_insert_list_after(struct list *lst, struct list_node *after, struct list *lst_to_insert)
{
	if (!list_is_empty(lst_to_insert)) {
		lst_to_insert->last->next = after->next;
		after->next = lst_to_insert->head.next;

		if (after == lst->last)
			lst->last = lst_to_insert->last;
	}
}


void *list_insert_last(struct list *lst, struct list_node *node)
{
	node->next = NULL;
	lst->last->next = node;
	lst->last = node;

	return node;
}


void *list_insert_first(struct list *lst, struct list_node *node)
{
	if (list_is_empty(lst))
		lst->last = node;

	node->next = lst->head.next;
	lst->head.next = node;

	return node;
}


void *list_remove_first(struct list *lst)
{
	struct list_node *first = list_first(lst);
	
	if (first) {
		lst->head.next = first->next;

		if (first == lst->last)
			lst->last = &lst->head;
	}

	return first;
}


void *list_remove_last(struct list *lst)
{
	struct list_node *last = lst->last;
	struct list_node *pred = list_find_predecessor(lst, lst->last);

	assert(pred != NULL);

	pred->next = NULL;
	lst->last = pred;

	return last;
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
	//node->next = NULL;

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


struct list_node *list_head(struct list *lst)
{
	return &lst->head;
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

	if (!list_is_empty(lst_to_prepend)) {
		first_prepend = list_first(lst_to_prepend);
		last_prepend = list_last(lst_to_prepend);

		assert(first_prepend != list_first(lst)); /* avoid obvious cycle */

		last_prepend->next = list_first(lst);
		lst->head.next = first_prepend;

		if (lst->last == &lst->head)
			lst->last = last_prepend;

		assert(list_is_empty(lst_to_prepend) || !list_is_empty(lst));
	}
}


void list_append(struct list *lst, struct list *lst_to_append)
{
	if (!list_is_empty(lst_to_append)) {
		lst->last->next = list_first(lst_to_append);
		lst->last = list_last(lst_to_append);
	}
}


void list_insert_list_before(struct list *lst, struct list *to_insert, struct list_node *node)
{
	assert(node != &lst->head);

	struct list_node *pred;

	if (!list_is_empty(to_insert)) {
		pred = list_find_predecessor(lst, node);
		pred->next = list_first(to_insert);
		((struct list_node *)list_last(to_insert))->next = node;
	}
}
