#ifndef NF_H_
#define NF_H_

#include "closure.h"

/* Reduce a closure to weak-head normal form, in place. That is, either a
 * primitive or an algebraic constructor, or a partially applied thunk.
 */
extern int whnf_closure(closure *clos);

#endif
