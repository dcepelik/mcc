#include "list.h"
#include "debug.h"
#include <assert.h>

void list_init(struct list *lst)
{
	lst->head.next = NULL;
	lst->last = &lst->head;
}

size_t list_len(struct list *lst)
{
	struct lnode *node;
	size_t len = 0;

	for (node = list_first(lst); node != NULL; node = lnode_next(node))
		len++;

	return len;
}

bool list_empty(struct list *lst)
{
	return lst->last == &lst->head;
}

void check_invariant(struct list *lst)
{
	DEBUG_EXPR("%lu", list_len(lst));
	DEBUG_EXPR("%i", list_empty(lst));
	struct lnode *n = list_first(lst), *m = NULL;
	while (n != NULL) {
		m = n;
		n = lnode_next(n);
	}
	assert((m == NULL && list_last(lst) == &lst->head) || m == list_last(lst));
	assert((list_len(lst) == 0 && list_empty(lst)) || (list_len(lst) > 0 && !list_empty(lst)));
}

void list_insert_after(struct list *lst, struct lnode *after, struct lnode *node)
{
	node->next = after->next;
	after->next = node;

	if (after == lst->last)
		lst->last = node;

	check_invariant(lst);
}

void list_insert_list_after(struct list *lst, struct lnode *after, struct list *lst_to_insert)
{
	if (!list_empty(lst_to_insert)) {
		lst_to_insert->last->next = after->next;
		after->next = lst_to_insert->head.next;

		if (after == lst->last)
			lst->last = lst_to_insert->last;
	}
}

void *list_insert(struct list *lst, struct lnode *node)
{
	DEBUG_TRACE;

	node->next = NULL;
	lst->last->next = node;
	lst->last = node;

	check_invariant(lst);
	return node;
}

void *list_insert_head(struct list *lst, struct lnode *node)
{
	DEBUG_TRACE;

	check_invariant(lst);

	if (list_empty(lst))
		lst->last = node;

	node->next = lst->head.next;
	lst->head.next = node;

	check_invariant(lst);
	return node;
}

void *list_remove_head(struct list *lst)
{
	DEBUG_TRACE;

	check_invariant(lst);
	struct lnode *first = list_first(lst);
	assert(first != NULL);
	
	lst->head.next = first->next;
	if (first == lst->last)
		lst->last = &lst->head;
	check_invariant(lst);
	return first;
}

void *list_remove(struct list *lst)
{
	DEBUG_TRACE;

	struct lnode *last = lst->last;
	struct lnode *pred = list_find_prev(lst, lst->last);

	assert(pred != NULL);

	pred->next = NULL;
	lst->last = pred;

	check_invariant(lst);
	return last;
}

void *list_find_prev(struct list *lst, struct lnode *node)
{
	struct lnode *cur;

	for (cur = &lst->head; cur != NULL; cur = lnode_next(cur)) {
		if (cur->next == node)
			break;
	}

	check_invariant(lst);
	return cur;
}

void *list_remove_node(struct list *lst, struct lnode *node)
{
	struct lnode *pred;

	pred = list_find_prev(lst, node);
	assert(pred != NULL);

	pred->next = node->next;
	//node->next = NULL;

	check_invariant(lst);
	return node;
}

struct list list_remove_range(struct list *lst, struct lnode *start, struct lnode *end)
{
	struct list rem_list;
	struct lnode *pred;

	pred = list_find_prev(lst, start);
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

struct lnode *list_head(struct list *lst)
{
	return &lst->head;
}

void *list_last(struct list *lst)
{
	return lst->last;
}

void *lnode_next(struct lnode *node)
{
	return node->next;
}

void list_free(struct list *lst)
{
	(void) lst;
}

void list_prepend(struct list *lst, struct list *lst_to_prepend)
{
	struct lnode *first_prepend;
	struct lnode *last_prepend;

	if (!list_empty(lst_to_prepend)) {
		first_prepend = list_first(lst_to_prepend);
		last_prepend = list_last(lst_to_prepend);

		assert(first_prepend != list_first(lst)); /* avoid obvious cycle */

		last_prepend->next = list_first(lst);
		lst->head.next = first_prepend;

		if (lst->last == &lst->head)
			lst->last = last_prepend;

		assert(list_empty(lst_to_prepend) || !list_empty(lst));
	}
}

void list_append(struct list *lst, struct list *lst_to_append)
{
	if (!list_empty(lst_to_append)) {
		lst->last->next = list_first(lst_to_append);
		lst->last = list_last(lst_to_append);
	}
}

void list_insert_list_before(struct list *lst, struct list *to_insert, struct lnode *node)
{
	assert(node != &lst->head);

	struct lnode *pred;

	if (!list_empty(to_insert)) {
		pred = list_find_prev(lst, node);
		pred->next = list_first(to_insert);
		((struct lnode *)list_last(to_insert))->next = node;
	}
}

bool list_contains(struct list *lst, struct lnode *node)
{
	struct lnode *n;
	for (n = list_first(lst); n != NULL; n = lnode_next(n))
		if (n == node)
			return true;
	return false;
}
