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
	newstr[sz] = 0;
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

extern void copy_array_sz(size_t sz, void **dest, size_t *destlen, void *const *src, size_t const *srclen)
{
	*destlen = *srclen;
	*dest = do_alloc(*destlen * sz);
	if(*destlen)
		memcpy(*dest, *src, *destlen * sz);
}

tree *new_tree(int tag, size_t num_children)
{
	tree *t = allocate(tree);
	t->tag = tag;
	t->children = allocate_arr(tree *, num_children);
	t->num_children = num_children;
	t->data = NULL;
	return t;
}

tree *new_tree_0_d(int tag, void *data)
{
	tree *t = new_tree(tag, 0);
	t->data = data;
	return t;
}

tree *new_tree_0(int tag)
{
	tree *t = new_tree(tag, 0);
	return t;
}

tree *new_tree_1(int tag, tree *child0)
{
	tree *t = new_tree(tag, 1);
	t->children[0] = child0;
	return t;
}

tree *new_tree_1_d(int tag, tree *child0, void *data)
{
	tree *t = new_tree(tag, 1);
	t->children[0] = child0;
	t->data = data;
	return t;
}

tree *new_tree_2(int tag, tree *child0, tree *child1)
{
	tree *t = new_tree(tag, 2);
	t->children[0] = child0;
	t->children[1] = child1;
	return t;
}

tree *new_tree_3(int tag, tree *child0, tree *child1, tree *child2)
{
	tree *t = new_tree(tag, 3);
	t->children[0] = child0;
	t->children[1] = child1;
	t->children[2] = child2;
	return t;
}

void free_tree(tree *t)
{
	if(t)
	{
		size_t i;
		for(i = 0; i < t->num_children; i++)
			free_tree(t->children[i]);
		unallocate(t->children);
		unallocate(t->data);
		unallocate(t);
	}
}
