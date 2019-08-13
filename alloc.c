#include <stdlib.h>

#include "util.h"

void *do_alloc(size_t size)
{
	void *ptr;
	if(!size)
		return NULL;
	ptr = malloc(size);
	if(!ptr)
		panic_errno("Could not allocate %lu bytes", (long unsigned)size);
	return ptr;
}

void do_free(void *ptr)
{
	if(ptr)
		free(ptr);
}

void *do_realloc(void *oldptr, size_t newsize)
{
	void *newptr;
	if(!oldptr)
		return do_alloc(newsize);
	if(!newsize) {
		do_free(oldptr);
		return NULL;
	}
	newptr = realloc(oldptr, newsize);
	if(!newptr)
		panic_errno("Could not allocate %lu bytes", (long unsigned)newsize);
	return newptr;
}
