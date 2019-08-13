#include <string.h>

#include "alloc.h"
#include "closure.h"
#include "util.h"

void erase_closure(closure *clos)
{
	ASSERT(clos);
	switch(clos->tag) {
	case CLOSURE_PRIM:
		unallocate(clos->u.prim.data);
		return;
	case CLOSURE_CONSTR:
		unallocate(clos->u.constr.fields);
		return;
	case CLOSURE_THUNK:
		unallocate(clos->u.thunk.env);
		return;
	default:
        panic("Unknown closure type %d", (int)clos->tag);
	}
}

static void free_masked(masked_entry *arr)
{
	if(arr) {
		masked_entry *ptr;
		for(ptr = arr; ptr->entry; ++ptr)
			unallocate(ptr->mask);
		unallocate(arr);
	}
}

void erase_entry(entry *ent)
{
	ASSERT(ent);
	switch(ent->tag) {
	case ENTRY_PRIM:
	case ENTRY_SELECT:
	case ENTRY_LAM:
		return;
	case ENTRY_APPLY:
		unallocate(ent->u.apply.fun.mask);
		unallocate(ent->u.apply.arg.mask);
		return;
	case ENTRY_CASE:
		unallocate(ent->u.caseof.scrutinee.mask);
		free_masked(ent->u.caseof.branches);
		return;
	case ENTRY_LETREC:
		unallocate(ent->u.letrec.body.mask);
		free_masked(ent->u.letrec.bindings);
		return;
	default:
		panic("Unknown entry type %d", (int)ent->tag);
	}
}

void copy_closure(closure *dest, closure *src)
{
	ASSERT(dest);
	ASSERT(src);
	if(dest == src) return;
	erase_closure(dest);
	dest->tag = src->tag;
	switch(src->tag) {
	case CLOSURE_PRIM:
		if(src->u.prim.data) {
			dest->u.prim.data = do_alloc(src->u.prim.size);
			memcpy(dest->u.prim.data, src->u.prim.data, src->u.prim.size);
		} else {
			dest->u.prim.data = NULL;
		}
		dest->u.prim.size = src->u.prim.size;
		return;
	case CLOSURE_CONSTR:
		dest->u.constr.var = src->u.constr.var;
		dest->u.constr.want_arity = src->u.constr.want_arity;
		if(src->u.constr.fields) {
			size_t cnt = 0, i;
			while(src->u.constr.fields[cnt])
				++cnt;
			dest->u.constr.fields = allocate_arr(closure *, cnt + 1);
			for(i = 0; i <= cnt; ++i)
				dest->u.constr.fields[i] = src->u.constr.fields[i];
		} else {
			dest->u.constr.fields = NULL;
		}
		return;
	case CLOSURE_THUNK:
		dest->u.thunk.entry = src->u.thunk.entry;
		dest->u.thunk.want_arity = src->u.thunk.want_arity;
		if(src->u.thunk.env) {
			size_t cnt = 0, i;
			while(src->u.thunk.env[cnt])
				++cnt;
			dest->u.thunk.env = allocate_arr(closure *, cnt + 1);
			for(i = 0; i <= cnt; ++i)
				dest->u.thunk.env[i] = src->u.thunk.env[i];
		} else {
			dest->u.thunk.env = NULL;
		}
		return;
	default:
        panic("Unknown closure type %d", (int)src->tag);
	}
}
