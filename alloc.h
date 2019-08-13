#ifndef ALLOC_H_
#define ALLOC_H_

#include <stdlib.h>

/* Returns NULL if size is 0 */
extern void *do_alloc(size_t);

/* If argument is NULL it is assumed to be an allocation of size 0.
 * Returns NULL if new size is 0.
 */
extern void *do_realloc(void *, size_t);

/* Does nothing if argument is NULL */
extern void do_free(void *);

#define allocate(T) ((T *)do_alloc(sizeof(T)))

#define allocate_arr(T, n) ((T *)do_alloc(sizeof(T) * (n)))

#define reallocate_arr(T, p, n) ((T *)do_realloc((p), sizeof(T) * (n)))

#define unallocate(p) do_free(p)

#endif
