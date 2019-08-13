#ifndef DATA_H_
#define DATA_H_

#include "alloc.h"

typedef struct ptr_list {
	void *ptr;
	struct ptr_list *next;
} *ptr_list;

extern void prepend_list(ptr_list *list, void *ptr);

extern void erase_list(ptr_list *list);

#endif
