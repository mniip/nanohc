#include <limits.h>

#include "alloc.h"
#include "closure.h"
#include "gc.h"
#include "nf.h"
#include "util.h"

static closure **mask_env(env_mask mask, closure **env)
{
	size_t popcnt = 0, i, j;
	closure **newenv;
	if(!env || !mask)
		return NULL;
	for(i = 0; env[i]; ++i)
		if(mask[i / CHAR_BIT] & (1 << (i % CHAR_BIT)))
			++popcnt;
	if(!popcnt)
		return NULL;
	newenv = allocate_arr(closure *, popcnt + 1);
	newenv[popcnt] = NULL;
	for(i = j = 0; env[i]; ++i)
		if(mask[i / CHAR_BIT] & (1 << (i % CHAR_BIT)))
			newenv[j++] = env[i];
	return newenv;
}

static closure **mask_concat_env(env_mask mask, closure **env1, closure **env2)
{
	size_t popcnt1 = 0, popcnt2 = 0, i, j;
	closure **newenv;
	if(!env1)
		return mask_env(mask, env2);
	if(!env2)
		return mask_env(mask, env1);
	for(i = 0; env1[i]; ++i)
		if(mask[i / CHAR_BIT] & (1 << (i % CHAR_BIT)))
			++popcnt1;
	for(i = 0; env2[i]; ++i)
		if(mask[(popcnt1 + i) / CHAR_BIT] & (1 << ((popcnt1 + i) % CHAR_BIT)))
			++popcnt2;
	if(!(popcnt1 + popcnt2))
		return NULL;
	newenv = allocate_arr(closure *, popcnt1 + popcnt2 + 1);
	newenv[popcnt1 + popcnt2] = NULL;
	for(i = j = 0; env1[i]; ++i)
		if(mask[i / CHAR_BIT] & (1 << (i % CHAR_BIT)))
			newenv[j++] = env1[i];
	for(i = 0; env2[i]; ++i)
		if(mask[(popcnt1 + i) / CHAR_BIT] & (1 << ((popcnt1 + i) % CHAR_BIT)))
			newenv[j++] = env2[i];
	return newenv;
}

static closure **extend_env(closure **env, closure *arg)
{
	closure **newenv;
	if(env) {
		size_t cnt = 0, i;
		while(env[cnt])
			++cnt;
		newenv = allocate_arr(closure *, cnt + 2);
		newenv[cnt + 1] = NULL;
		newenv[cnt] = arg;
		for(i = 0; i < cnt; ++i)
			newenv[i] = env[i];
	} else {
		newenv = allocate_arr(closure *, 2);
		newenv[1] = NULL;
		newenv[0] = arg;
	}
	return newenv;
}

static int materialize(closure *self, closure **env, entry *ent);

/* Same as materialize but take ownership of the environment */
static void materialize_free_env(closure *self, closure **env, entry *ent)
{
	materialize(self, env, ent);
	unallocate(env);
}

/* Apply the given function to the given argument and reduce the result to whnf
 * placing it into the given closure. Takes ownership of the function and the
 * argument closure.
 */
static int apply(closure *self, closure *fun, closure *arg)
{
	ASSERT(gc_live_closure(self));
	ASSERT(gc_live_closure(fun));
	ASSERT(gc_live_closure(arg));
	whnf_closure(fun);
	switch(fun->tag) {
	case CLOSURE_CONSTR:
		ASSERT(self->u.constr.want_arity);
		erase_closure(self);
		self->tag = CLOSURE_CONSTR;
		self->u.constr.var = fun->u.constr.var;
		self->u.constr.want_arity = fun->u.constr.want_arity - 1;
		self->u.constr.fields = extend_env(fun->u.constr.fields, arg);
		gc_unuse_closure(fun);
		gc_unuse_closure(arg);
		return 0;
	case CLOSURE_THUNK:
		{
			closure **newenv;
			entry *newent = fun->u.thunk.entry;
			ASSERT(fun->u.thunk.want_arity);
			if(fun->u.thunk.want_arity == 1) {
				newenv = extend_env(fun->u.thunk.env, arg);
				gc_unuse_closure(fun);
				gc_unuse_closure(arg);
				materialize_free_env(self, newenv, newent);
			} else {
				erase_closure(self);
				self->tag = CLOSURE_THUNK;
				self->u.thunk.entry = fun->u.thunk.entry;
				self->u.thunk.want_arity = fun->u.thunk.want_arity - 1;
				self->u.thunk.env = extend_env(fun->u.thunk.env, arg);
				gc_unuse_closure(fun);
				gc_unuse_closure(arg);
			}
			return 0;
		}
	default:
		panic("Invalid closure type for apply %d", (int)fun->tag);
		return 0;
	}
}

/* Evaluate the given entry code in the given environment, storing the result
 * in the given closure. It is assumed that the closure is provided used, and
 * that the environment and the entry code might be invalidated when something
 * else is entered. Return int so we can tail call.
 */
static int materialize(closure *self, closure **env, entry *ent)
{
	ASSERT(ent);
	ASSERT(gc_live_closure(self));
	ASSERT(gc_live_entry(ent));
	switch(ent->tag) {
	case ENTRY_PRIM:
		ASSERT(self->tag == CLOSURE_THUNK && self->u.thunk.env == env);
		return ent->u.prim(self);
	case ENTRY_REF:
		{
			closure *ref = ent->u.ref;
			gc_use_closure(ref);
			whnf_closure(ref); /* TODO: tail call */
			copy_closure(self, ref);
			gc_unuse_closure(ref);
			return 0;
		}
	case ENTRY_SELECT:
		{
			closure *tgt;
			ASSERT(env);
			tgt = env[ent->u.select_idx];
			gc_use_closure(tgt);
			whnf_closure(tgt);
			copy_closure(self, tgt);
			gc_unuse_closure(tgt);
			return 0;
		}
	case ENTRY_APPLY:
		{
			closure *fun, *arg;
			fun = new_closure(CLOSURE_THUNK);
			fun->u.thunk.want_arity = 0;
			fun->u.thunk.env = mask_env(ent->u.apply.fun.mask, env);
			fun->u.thunk.entry = ent->u.apply.fun.entry;
			arg = new_closure(CLOSURE_THUNK);
			arg->u.thunk.want_arity = 0;
			arg->u.thunk.env = mask_env(ent->u.apply.arg.mask, env);
			arg->u.thunk.entry = ent->u.apply.arg.entry;
			return apply(self, fun, arg);
		}
	case ENTRY_CASE:
		{	
			/* TODO: what if self is overwritten */
			closure *scrut = new_closure(-1);
			closure **newenv;
			entry *newent;
			variant var;
			materialize_free_env(scrut,
				mask_env(ent->u.caseof.scrutinee.mask, env),
				ent->u.caseof.scrutinee.entry);
			ASSERT(scrut->tag == CLOSURE_CONSTR && !scrut->u.constr.want_arity);
			var = scrut->u.constr.var;
			newenv = mask_concat_env(ent->u.caseof.branches[var].mask,
				env, scrut->u.constr.fields);
			newent = ent->u.caseof.branches[var].entry;
			gc_unuse_closure(scrut);
			materialize_free_env(self, newenv, newent);
			return 0;
		}
	case ENTRY_LETREC:
		{
			closure **bindings;
			closure **newenv;
			entry *newent = ent->u.letrec.body.entry;
			size_t cnt = 0, i;
			while(ent->u.letrec.bindings[cnt].entry)
				++cnt;
			bindings = allocate_arr(closure *, cnt + 1);
			bindings[cnt] = NULL;
			for(i = 0; i < cnt; ++i)
				bindings[i] = new_closure(CLOSURE_NULL);
			for(i = 0; i < cnt; ++i) {
				bindings[i]->tag = CLOSURE_THUNK;
				bindings[i]->u.thunk.want_arity = 0;
				bindings[i]->u.thunk.env = mask_concat_env(
					ent->u.letrec.bindings[i].mask, env, bindings);
				bindings[i]->u.thunk.entry = ent->u.letrec.bindings[i].entry;
			}
			newenv = mask_concat_env(ent->u.letrec.body.mask, env, bindings);
			unallocate(bindings);
			materialize_free_env(self, newenv, newent);
			return 0;
		}
	case ENTRY_LAM:
		ASSERT(self->tag == CLOSURE_THUNK && self->u.thunk.env == env);
		self->u.thunk.want_arity = 1;
		self->u.thunk.entry = ent->u.lambda.body;
		return 0;
	default:
		panic("Unknown entry type %d", (int)ent->tag);
		return 0;
	}
}

/* Return int so we can tail call */
int whnf_closure(closure *self)
{
	ASSERT(self);
	ASSERT(gc_live_closure(self));
	switch(self->tag) {
	case CLOSURE_PRIM:
	case CLOSURE_CONSTR:
		return 0;
	case CLOSURE_THUNK:
		gc_use_closure(self);
		if(self->u.thunk.want_arity)
			return 0;
		return materialize(self, self->u.thunk.env, self->u.thunk.entry);
	default:
		panic("Unknown closure type %d", (int)self->tag);
		return 0;
	}
}
