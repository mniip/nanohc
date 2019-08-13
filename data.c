#include "data.h"

void prepend_list(ptr_list *list, void *ptr)
{
	ptr_list newl = allocate(struct ptr_list);
	newl->ptr = ptr;
	newl->next = *list;
	*list = newl;
}

void erase_list(ptr_list *list)
{
	ptr_list nextl = (*list)->next;
	unallocate(*list);
	*list = nextl;
}
