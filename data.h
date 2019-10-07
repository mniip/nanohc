#ifndef DATA_H_
#define DATA_H_

#include "alloc.h"

/* A linked list of void * pointers */
typedef struct ptr_list {
	void *ptr;
	struct ptr_list *next;
} *ptr_list;

extern void prepend_list(ptr_list *list, void *ptr);
extern void erase_list(ptr_list *list);

/* Copy and return a unique pointer representing the string */
extern char const *intern(char const *str);
extern char const *intern_sz(char const *str, size_t sz);

/* Variable length arrays of elements of type T: */
/* Deallocate an array */
#define free_array(T, array, len) free_array_sz(sizeof(T), (void **)array, len)
/* Add an element at the end */
#define grow_array(T, array, len) grow_array_sz(sizeof(T), (void **)array, len)
/* Remove an element from the end */
#define shrink_array(T, array, len) shrink_array_sz(sizeof(T), (void **)array, len)
/* Remove an element at specified index */
#define remove_array(T, array, len) remove_array_sz(sizeof(T), (void **)array, len)
#define copy_array(T, dest, destlen, src, srclen) copy_array_sz(sizeof(T), (void **)dest, destlen, (void **)src, srclen)

extern void free_array_sz(size_t sz, void **array, size_t *len);
extern void grow_array_sz(size_t sz, void **array, size_t *len);
extern void shrink_array_sz(size_t sz, void **array, size_t *len);
extern void remove_array_sz(size_t sz, void **array, size_t *len, int at);
extern void copy_array_sz(size_t sz, void **dest, size_t *destlen, void *const *str, size_t const *srclen);

typedef struct tree {
	int tag;
	struct tree **children;
	size_t num_children;
	void *data;
} tree;

/* Allocate a node of a given arity */
extern tree *new_tree(int tag, size_t num_children);

/* Allocate a tree with 0 children and given data */
extern tree *new_tree_0_d(int tag, void *data);

/* Allocate a tree with 0 children */
extern tree *new_tree_0(int tag);

/* Allocate a tree with 1 child */
extern tree *new_tree_1(int tag, tree *);

/* Allocate a tree with 1 child and given data */
extern tree *new_tree_1_d(int tag, tree *, void *data);

/* Allocate a tree with 2 children */
extern tree *new_tree_2(int tag, tree *, tree *);

/* Allocate a tree with 3 children */
extern tree *new_tree_3(int tag, tree *, tree *, tree *);

/* Deallocate the entire tree recursively */
extern void free_tree(tree *t);

#endif
