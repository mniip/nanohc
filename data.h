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

extern void free_array_sz(size_t sz, void **array, size_t *len);
extern void grow_array_sz(size_t sz, void **array, size_t *len);
extern void shrink_array_sz(size_t sz, void **array, size_t *len);
extern void remove_array_sz(size_t sz, void **array, size_t *len, int at);

#endif
