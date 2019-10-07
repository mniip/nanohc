#ifndef CLOSURE_H_
#define CLOSURE_H_

#include <stddef.h>

enum closure_tag {
	CLOSURE_NULL = 0x00,
	CLOSURE_PRIM,
	CLOSURE_CONSTR,
	CLOSURE_THUNK
};

typedef unsigned char variant;
typedef unsigned char gc_data;
typedef unsigned short int arity;

/* A heap-allocated closure, managed by the GC */
typedef struct closure {
	char tag;
	gc_data gc;
	union {
		/* tag = CLOSURE_PRIM, an evaluated primitive datatype value
		 * prim points to an arbitrarily sized allocation owned by the closure
		 */
		struct {
			void *data;
			size_t size;
		} prim;
		/* tag = CLOSURE_CONSTR, an evaluated algebraic datatype value */
		struct {
			/* Variant of the constructor */
			variant var;
			/* How many arguments is the constructor missing, if partially
			 * applied. 0 means it's fully applied.
			 */
			arity want_arity;
			/* A NULL-terminated list of fields of the constructor, allocation
			 * owned by the closure.
			 */
			struct closure **fields;
		} constr;
		/* tag = CLOSURE_THUNK, either a thunk or a lambda */
		struct {
			/* How many arguments is the thunk missing. 0 means it's a thunk not
			 * in WHNF. >0 means it's a lambda in WHNF.
			 */
			arity want_arity;
			/* A NULL-terminated list of the values that the closure is closed
			 * over, allocation owned by the closure.
			 */
			struct closure **env;
			/* Entry "code" */
			struct entry *entry;
		} thunk;
	} u;
} closure;

enum entry_tag {
	ENTRY_PRIM = 0x01,
	ENTRY_REF,
	ENTRY_SELECT,
	ENTRY_APPLY,
	ENTRY_CASE,
	ENTRY_LETREC,
	ENTRY_LAM
};

/* A bitmask specifying a subset of the environment to be passed to the child
 * entry code.
 */
typedef unsigned char *env_mask;

typedef struct masked_entry {
	/* Pointer never shared */
	env_mask mask;
	struct entry *entry;
} masked_entry;

/* Entry code for a thunk or function, instructions on how to replace the
 * closure with an evaluated version of itself. Managed by the GC.
 */
typedef struct entry {
	char tag;
	gc_data gc;
	union {
		/* tag = ENTRY_PRIM, apply a primitive function */
		int (*prim)(closure *self);
		/* tag = ENTRY_REF, refer to another closure */
		closure *ref;
		/* tag = ENTRY_SELECT, select a closure from the environemnt by index */
		arity select_idx;
		/* tag = ENTRY_APPLY, apply a function to a thunk */
		struct {
			/* How to construct the function */
			masked_entry fun;
			/* How to construct the argument */
			masked_entry arg;
		} apply;
		/* tag = ENTRY_CASE, perform case analysis of expression and return
		 * one of possible branches.
		 */
		struct {
			/* How to construct the scrutinee */
			masked_entry scrutinee;
			/* A NULL-terminated list of case branches, as many as as there are
			 * variants of the matched datatype.
			 */
			masked_entry *branches;
		} caseof;
		/* tag = ENTRY_LETREC, add some closures to the environment */
		struct {
			/* How to construct the body once other closures have been bound */
			masked_entry body;
			/* A NULL-terminated list of bindings. */
			masked_entry *bindings;
		} letrec;
		/* tag = ENTRY_LAM, a lambda, request an additional argument */
		struct {
			/* How to construct the body once applied to an argument */
			struct entry *body;
		} lambda;
	} u;
} entry;

/* Deallocate all data in a closure (so that it can be replaced with new data)
 */
extern void erase_closure(closure *);

/* Deallocate all data in an entry */
extern void erase_entry(entry *);

/* Replace all data of one closure by that of another */
extern void copy_closure(closure *dest, closure *src);

#endif
