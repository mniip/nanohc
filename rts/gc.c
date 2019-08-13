#include "data.h"
#include "gc.h"
#include "util.h"

/* List of all allocated closures */
static ptr_list gc_closure_list = NULL;
size_t gc_closure_list_sz = 0;
/* List of all allocated entries */
static ptr_list gc_entry_list = NULL;
size_t gc_entry_list_sz = 0;

/* Sum of the two list sizes after last collection */
size_t last_collection = 0;

/* During a walk indicates that we have visited this node */
#define GC_SEEN   0x01
/* Indicates a node currently being manipulated */
#define GC_USED   0x02
/* Indicates a "root", such as a global binding */
#define GC_PINNED 0x04
#define GC_REFERRED (GC_USED | GC_PINNED)
#define GC_DEAD   0xF8

void gc_pin(closure *clos) { clos->gc |= GC_PINNED; }
void gc_unpin(closure *clos) { clos->gc &= ~GC_PINNED; }

void gc_use_entry(entry *ent) { ent->gc |= GC_USED; }
void gc_unuse_entry(entry *ent) { ent->gc &= ~GC_USED; }
void gc_use_closure(closure *clos) { clos->gc |= GC_USED; }
void gc_unuse_closure(closure *clos) { clos->gc &= ~GC_USED; }

int gc_live_closure(closure *clos) { return !(clos->gc & GC_DEAD); }
int gc_live_entry(entry *ent) { return !(ent->gc & GC_DEAD); }

/* Simple depth-first mark-and-sweep */
static int walk_closure(closure *);

/* Return int so we can tail call */
static int walk_entry(entry *ent)
{
	ASSERT(ent);
	ASSERT(!(ent->gc & GC_DEAD));
	if(ent->gc & GC_SEEN) return 0;
	ent->gc |= GC_SEEN;
	switch(ent->tag) {
	case ENTRY_PRIM:
	case ENTRY_SELECT:
		return 0;
	case ENTRY_REF:
		return walk_closure(ent->u.ref);
	case ENTRY_APPLY:
		walk_entry(ent->u.apply.fun.entry);
		return walk_entry(ent->u.apply.arg.entry);
	case ENTRY_CASE:
		if(ent->u.caseof.branches) {
			masked_entry *ptr;
			for(ptr = ent->u.caseof.branches; ptr->entry; ++ptr)
				walk_entry(ptr->entry);
		}
		return walk_entry(ent->u.caseof.scrutinee.entry);
	case ENTRY_LETREC:
		if(ent->u.letrec.bindings) {
			masked_entry *ptr;
			for(ptr = ent->u.letrec.bindings; ptr->entry; ++ptr)
				walk_entry(ptr->entry);
		}
		return walk_entry(ent->u.letrec.body.entry);
	case ENTRY_LAM:
		return walk_entry(ent->u.lambda.body);
	default:
		panic("Unknown entry type %d", (int)ent->tag);
		return 0;
	}
}

/* Return int so we can tail call */
static int walk_closure(closure *clos)
{
	ASSERT(clos);
	ASSERT(!(clos->gc & GC_DEAD));
	if(clos->gc & GC_SEEN) return 0;
	clos->gc |= GC_SEEN;
	switch(clos->tag) {
	case CLOSURE_NULL:
		ASSERT(clos->gc & GC_USED);
		return 0;
	case CLOSURE_PRIM:
		return 0;
	case CLOSURE_CONSTR:
		if(clos->u.constr.fields) {
			closure **ptr;
			for(ptr = clos->u.constr.fields; *ptr; ++ptr)
				walk_closure(*ptr);
		}
		return 0;
	case CLOSURE_THUNK:
		if(clos->u.thunk.env) {
			closure **ptr;
			for(ptr = clos->u.thunk.env; *ptr; ++ptr)
				walk_closure(*ptr);
		}
		return walk_entry(clos->u.thunk.entry);
	default:
		panic("Unknown closure type %d", (int)clos->tag);
		return 0;
	}
}

static void free_closure(closure *clos)
{
	erase_closure(clos);
	clos->gc = ~0;
	unallocate(clos);
}

static void free_entry(entry *ent)
{
	erase_entry(ent);
	ent->gc = ~0;
	unallocate(ent);
}

void gc_collect()
{
	ptr_list *plst, lst;
	/* Mark */
	for(lst = gc_closure_list; lst; lst = lst->next)
		if(((closure *)lst->ptr)->gc & GC_REFERRED)
			walk_closure((closure *)lst->ptr);
	/* Sweep */
	for(plst = &gc_closure_list; *plst; )
		if(((closure *)(*plst)->ptr)->gc & GC_SEEN) {
			((closure *)(*plst)->ptr)->gc &= ~GC_SEEN;
			plst = &(*plst)->next;
		} else {
			free_closure((closure *)(*plst)->ptr);
			erase_list(plst);
			--gc_closure_list_sz;
		}
	for(plst = &gc_entry_list; *plst; )
		if(((entry *)(*plst)->ptr)->gc & GC_SEEN) {
			((entry *)(*plst)->ptr)->gc &= ~GC_SEEN;
			plst = &(*plst)->next;
		} else {
			free_entry((entry *)(*plst)->ptr);
			erase_list(plst);
			--gc_entry_list_sz;
		}
	last_collection = gc_closure_list_sz + gc_entry_list_sz;
}

closure *new_closure(char tag)
{
	closure *clos;
	if(gc_closure_list_sz + gc_entry_list_sz > 2 * last_collection)
		gc_collect();

	clos = allocate(closure);
	clos->tag = tag;
	clos->gc = GC_USED;
	prepend_list(&gc_closure_list, clos);
	++gc_closure_list_sz;
	return clos;
}

entry *new_entry(char tag)
{
	entry *ent;
	if(gc_closure_list_sz + gc_entry_list_sz > 2 * last_collection)
		gc_collect();

	ent = allocate(entry);
	ent->tag = tag;
	ent->gc = GC_USED;
	prepend_list(&gc_entry_list, ent);
	++gc_entry_list_sz;
	return ent;
}
