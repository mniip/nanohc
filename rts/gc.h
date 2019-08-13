#ifndef GC_H_
#define GC_H_

#include "closure.h"

extern closure *new_closure(char tag);

extern entry *new_entry(char tag);

/* Pin/unpin a GC "root" */
extern void gc_pin(closure *);
extern void gc_unpin(closure *);

/* Temporarily mark an object as being used */
extern void gc_use_entry(entry *);
extern void gc_unuse_entry(entry *);
extern void gc_use_closure(closure *);
extern void gc_unuse_closure(closure *);

/* Diagnostic checks whether a pointer hasn't been deallocated */
extern int gc_live_closure(closure *);
extern int gc_live_entry(entry *);

extern void gc_collect();

#endif
