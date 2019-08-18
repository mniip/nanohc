#include <string.h>

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

ptr_list interned_strings;

char const *intern_sz(char const *str, size_t sz)
{
	ptr_list ptr;
	char *newstr;
	for(ptr = interned_strings; ptr; ptr = ptr->next)
		if(!strncmp((char *)ptr->ptr, str, sz))
			if(strlen((char *)ptr->ptr) == sz)
				return (char *)ptr->ptr;
	newstr = allocate_arr(char, sz + 1);
	strncpy(newstr, str, sz);
	prepend_list(&interned_strings, newstr);
	return interned_strings->ptr;
}

char const *intern(char const *str)
{
	ptr_list ptr;
	char *newstr;
	for(ptr = interned_strings; ptr; ptr = ptr->next)
		if(strcmp((char *)ptr->ptr, str))
			return (char *)ptr->ptr;
	newstr = allocate_arr(char, strlen(str) + 1);
	strcpy(newstr, str);
	prepend_list(&interned_strings, newstr);
	return interned_strings->ptr;
}

void realloc_array_sz(size_t sz, void **array, size_t *len)
{
	*array = do_realloc(*array, *len * sz);
}

void free_array_sz(size_t sz, void **array, size_t *len)
{
	(void)sz;
	do_free(*array);
	*array = NULL;
	*len = 0;
}

void grow_array_sz(size_t sz, void **array, size_t *len)
{
	(*len)++;
	realloc_array_sz(sz, array, len);
}

void shrink_array_sz(size_t sz, void **array, size_t *len)
{
	(*len)--;
	realloc_array_sz(sz, array, len);
}

void remove_array_sz(size_t sz, void **array, size_t *len, int at)
{
	memmove(
		*(char **)array + at * sz,
		*(char **)array + (at + 1) * sz,
		(*len - at - 1) * sz);
	shrink_array_sz(sz, array, len);
}
