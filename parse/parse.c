#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "data.h"
#include "lex.h"
#include "parse.h"
#include "util.h"

void parser_new(parser *p, char const *data)
{
	lexer_new(&p->l, data);
	p->depth = 0;
}

void parser_eof(parser *p)
{
	token tk;
	lexer_next(&p->l, &tk);
	if(tk.type != TK_EOF)
		panic("%d:%d: Did not consume the entire input",
			(int)tk.line, (int)tk.col);
}

void parser_free(parser *p)
{
	lexer_free(&p->l);
}

struct parser_save {
	lexer l;
};

static void save_parser(parser *p, struct parser_save *s)
{
	lexer_copy(&s->l, &p->l);
}

static void restore_parser(parser *p, struct parser_save *s)
{
	lexer_free(&p->l);
	lexer_copy(&p->l, &s->l);
}

static void free_save(struct parser_save *s)
{
	lexer_free(&s->l);
}

static tree *run_parser(parser *p, tree *(*f)(parser *))
{
	tree *t;
	if(p->depth == MAX_DEPTH)
		panic("%d:%d: Ran out of depth",
			(int)p->l.line, (int)(p->l.pos - p->l.line_start));
	++p->depth;
	t = f(p);
	--p->depth;
	return t;
}

static tree *try_parser(parser *p, tree *(*f)(parser *))
{
	struct parser_save s;
	tree *t;
	save_parser(p, &s);
	t = run_parser(p, f);
	if(!t)
		restore_parser(p, &s);
	free_save(&s);
	return t;
}

static int is_con_name(char const *name)
{
	return (name[0] >= 'A' && name[0] <= 'Z') || name[0] == ':';
}

static tree *parse_semicolon_list(parser *p, tree *(*f)(parser *))
{
	token tk;
	tree *list = NULL;
	tree **list_end = &list;
	lexer_next(&p->l, &tk);
	while(tk.type == TK_SEMICOLON || tk.type == TK_VSEMICOLON)
		lexer_next(&p->l, &tk);
	lexer_unsee(&p->l, &tk);
	while(1) {
		tree *elem = try_parser(p, f);
		if(!elem)
			break;
		*list_end = new_tree_2(AST_CONS, elem, NULL);
		list_end = &(*list_end)->children[1];
		lexer_next(&p->l, &tk);
		if(tk.type != TK_SEMICOLON && tk.type != TK_VSEMICOLON) {
			lexer_unsee(&p->l, &tk);
			break;
		}
		while(tk.type == TK_SEMICOLON || tk.type == TK_VSEMICOLON)
			lexer_next(&p->l, &tk);
		lexer_unsee(&p->l, &tk);
	}
	*list_end = new_tree_0(AST_NIL);
	return list;
}

tree *parse_type(parser *);
static tree *parse_btype(parser *);
static tree *parse_atype(parser *);

static tree *parse_var(parser *);
static tree *parse_qvar(parser *);
static tree *parse_qcon(parser *);
static tree *parse_gcon(parser *);

tree *parse_exp(parser *);
static tree *parse_infixexp(parser *);
static tree *parse_lexp(parser *);
static tree *parse_fexp(parser *);
static tree *parse_qop(parser *);
static tree *parse_aexp(parser *);
static tree *parse_stmts(parser *);
static tree *parse_stmt(parser *);

static tree *parse_alts(parser *);
static tree *parse_alt(parser *);
static tree *parse_gdpat(parser *);
static tree *parse_guards(parser *);
static tree *parse_guard(parser *);

static tree *parse_pat(parser *);
static tree *parse_apat(parser *);

tree *parse_topdecls(parser *);
static tree *parse_topdecl(parser *);
static tree *parse_decls(parser *);
static tree *parse_rhs(parser *);
static tree *parse_gdrhs(parser *);
static tree *parse_decl(parser *);

tree *parse_type(parser *p)
{
	tree *lhs = run_parser(p, parse_btype);
	token tk;
	if(!lhs) return NULL;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_TO) {
		tree *rhs = run_parser(p, parse_type);
		if(!rhs) return free_tree(lhs), NULL;
		return
			new_tree_2(AST_TYPE_APPLY,
				new_tree_2(AST_TYPE_APPLY,
					new_tree_0(AST_TYPE_ARROW),
					lhs),
				rhs);
	} else {
		lexer_unsee(&p->l, &tk);
		return lhs;
	}
}

static tree *parse_btype(parser *p)
{
	tree *lhs = run_parser(p, parse_atype);
	if(!lhs) return NULL;
	while(1) {
		tree *rhs = try_parser(p, parse_atype);
		if(!rhs)
			return lhs;
		lhs = new_tree_2(AST_TYPE_APPLY, lhs, rhs);
	}
}

static tree *make_qualname(qualname name)
{
	qualname *q = allocate(qualname);
	q->qualifier = name.qualifier;
	q->name = name.name;
	return new_tree_0_d(AST_QUALNAME, q);
}

static tree *make_name(qualname name)
{
	char const **ptr = allocate(char const *);
	if(name.qualifier)
		panic("Expected an unqualified name");
	*ptr = name.name;
	return new_tree_0_d(AST_NAME, ptr);
}

static void *make_int(unsigned long int i)
{
	size_t *p = allocate(unsigned long int);
	*p = i;
	return p;
}

static tree *parse_atype(parser *p)
{
	token tk;
	lexer_next(&p->l, &tk);
	switch(tk.type) {
	case TK_OPENPAREN:
		lexer_next(&p->l, &tk);
		switch(tk.type) {
		case TK_CLOSEPAREN:
			return new_tree_1(AST_TYPE_CON,
				new_tree_0_d(AST_TUPLE, make_int(0)));
		case TK_COMMA:
			{
				size_t arity = 1;
				while(tk.type == TK_COMMA) {
					++arity;
					lexer_next(&p->l, &tk);
				}
				if(tk.type == TK_CLOSEPAREN)
					return new_tree_1(AST_TYPE_CON,
						new_tree_0_d(AST_TUPLE, make_int(arity)));
				lexer_unsee(&p->l, &tk);
				return NULL;
			}
		case TK_TO:
			lexer_next(&p->l, &tk);
			if(tk.type == TK_CLOSEPAREN)
				return new_tree_0(AST_TYPE_ARROW);
			lexer_unsee(&p->l, &tk);
			return NULL;
		default:
			{
				unsigned long int *arity = make_int(0);
				tree *fun = new_tree_1(AST_TYPE_CON,
					new_tree_0_d(AST_TUPLE, arity));
				tree *arg;
				lexer_unsee(&p->l, &tk);
				arg = run_parser(p, parse_type);
				if(!arg) return free_tree(fun), NULL;
				++*arity;
				fun = new_tree_2(AST_TYPE_APPLY, fun, arg);
				while(1) {
					lexer_next(&p->l, &tk);
					if(tk.type == TK_CLOSEPAREN)
						break;
					else if(tk.type != TK_COMMA)
						return lexer_unsee(&p->l, &tk), free_tree(fun), NULL;
					arg = run_parser(p, parse_type);
					if(!arg) return free_tree(fun), NULL;
					++*arity;
					fun = new_tree_2(AST_TYPE_APPLY, fun, arg);
				}
				if(*arity == 1) {
					tree *expr = fun->children[1];
					fun->children[1] = NULL;
					free_tree(fun);
					return expr;
				}
				return fun;
			}
		}
	case TK_NAME:
		if(is_con_name(tk.u.name.name))
			return new_tree_1(AST_TYPE_CON, make_qualname(tk.u.name));
		else
			return new_tree_1(AST_TYPE_VAR, make_name(tk.u.name));
	default:
		lexer_unsee(&p->l, &tk);
		return NULL;
	}
}

static tree *parse_var(parser *p)
{
	token tk;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_OPENPAREN) {
		tree *con;
		lexer_next(&p->l, &tk);
		if(tk.type != TK_SYMBOL || is_con_name(tk.u.name.name))
			return lexer_unsee(&p->l, &tk), NULL;
		con = make_name(tk.u.name);
		lexer_next(&p->l, &tk);
		if(tk.type != TK_CLOSEPAREN)
			return lexer_unsee(&p->l, &tk), free_tree(con), NULL;
		return con;
	} else if(tk.type == TK_NAME) {
		if(!is_con_name(tk.u.name.name))
			return make_name(tk.u.name);
	}
	lexer_unsee(&p->l, &tk);
	return NULL;
}

static tree *parse_qvar(parser *p)
{
	token tk;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_OPENPAREN) {
		tree *con;
		lexer_next(&p->l, &tk);
		if(tk.type != TK_SYMBOL || is_con_name(tk.u.name.name))
			return lexer_unsee(&p->l, &tk), NULL;
		con = make_qualname(tk.u.name);
		lexer_next(&p->l, &tk);
		if(tk.type != TK_CLOSEPAREN)
			return lexer_unsee(&p->l, &tk), free_tree(con), NULL;
		return con;
	} else if(tk.type == TK_NAME) {
		if(!is_con_name(tk.u.name.name))
			return make_qualname(tk.u.name);
	}
	lexer_unsee(&p->l, &tk);
	return NULL;
}

static tree *parse_qcon(parser *p)
{
	token tk;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_OPENPAREN) {
		tree *con;
		lexer_next(&p->l, &tk);
		if(tk.type != TK_SYMBOL || !is_con_name(tk.u.name.name))
			return lexer_unsee(&p->l, &tk), NULL;
		con = make_qualname(tk.u.name);
		lexer_next(&p->l, &tk);
		if(tk.type != TK_CLOSEPAREN)
			return lexer_unsee(&p->l, &tk), free_tree(con), NULL;
		return con;
	} else if(tk.type == TK_NAME) {
		if(is_con_name(tk.u.name.name))
			return make_qualname(tk.u.name);
	}
	lexer_unsee(&p->l, &tk);
	return NULL;
}

static tree *parse_gcon(parser *p)
{
	token tk;
	tree *con = try_parser(p, parse_qcon);
	if(con) return con;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_OPENPAREN) {
		lexer_next(&p->l, &tk);
		if(tk.type == TK_CLOSEPAREN) {
			return new_tree_0_d(AST_TUPLE, make_int(0));
		} else if(tk.type == TK_COMMA) {
			size_t arity = 1;
			while(tk.type == TK_COMMA) {
				++arity;
				lexer_next(&p->l, &tk);
			}
			if(tk.type == TK_CLOSEPAREN)
				return new_tree_0_d(AST_TUPLE, make_int(arity));
		}
	} else if(tk.type == TK_OPENBRACKET) {
		qualname nil;
		lexer_next(&p->l, &tk);
		if(tk.type != TK_CLOSEBRACKET)
			return lexer_unsee(&p->l, &tk), NULL;
		nil.qualifier = NULL;
		nil.name = intern("[]");
		return make_qualname(nil);
	}
	lexer_unsee(&p->l, &tk);
	return NULL;
}

tree *parse_exp(parser *p)
{
	tree *expr = run_parser(p, parse_infixexp);
	token tk;
	if(!expr) return NULL;
	while(1) {
		lexer_next(&p->l, &tk);
		if(tk.type == TK_HASTYPE) {
			tree *ty = run_parser(p, parse_type);
			if(!ty) return free_tree(expr), NULL;
			expr = new_tree_2(AST_CAST, expr, ty);
		} else {
			lexer_unsee(&p->l, &tk);
			return expr;
		}
	}
}

static tree *parse_infixexp(parser *p)
{
	struct parser_save s;
	tree *arg1, *op, *arg2;
	arg1 = run_parser(p, parse_lexp);
	if(!arg1) return NULL;
	save_parser(p, &s);
	op = run_parser(p, parse_qop);
	if(!op) return restore_parser(p, &s), free_save(&s), arg1;
	arg2 = run_parser(p, parse_infixexp);
	if(!arg2) return free_tree(op), restore_parser(p, &s), free_save(&s), arg1;
	return new_tree_3(AST_UOPERATOR, op, arg1, arg2);
}

static tree *parse_lexp(parser *p)
{
	token tk;
	lexer_next(&p->l, &tk);
	switch(tk.type) {
	case TK_LAMBDA:
		{
			tree *patlist = NULL;
			tree **patlist_end = &patlist;
			while(1) {
				lexer_next(&p->l, &tk);
				if(tk.type == TK_TO) {
					tree *expr = run_parser(p, parse_exp);
					if(!expr) return free_tree(patlist), NULL;
					*patlist_end = new_tree_0(AST_NIL);
					return new_tree_2(AST_LAMBDA, patlist, expr);
				} else {
					tree *pat;
					lexer_unsee(&p->l, &tk);
					pat = run_parser(p, parse_apat);
					if(!pat) return free_tree(patlist), NULL;
					*patlist_end = new_tree_2(AST_CONS, pat, NULL);
					patlist_end = &(*patlist_end)->children[1];
				}
			}
		}
	case TK_LET:
		{
			tree *decls;
			int virt = lexer_next_open(&p->l, &tk);
			if(tk.type != TK_OPENBRACE && tk.type != TK_VOPENBRACE)
				return lexer_unsee(&p->l, &tk), NULL;
			decls = run_parser(p, parse_decls);
			if(!decls) return NULL;
			lexer_next_close(&p->l, &tk, virt);
			if(tk.type != (virt ? TK_VCLOSEBRACE : TK_CLOSEBRACE))
				return lexer_unsee(&p->l, &tk), free_tree(decls), NULL;
			lexer_next(&p->l, &tk);
			if(tk.type != TK_IN) {
				return lexer_unsee(&p->l, &tk), free_tree(decls), NULL;
			} else {
				tree *expr = run_parser(p, parse_exp);
				if(!expr) return free_tree(decls), expr;
				return new_tree_2(AST_LET, decls, expr);
			}
		}
	case TK_IF:
		{
			tree *cond, *expr1, *expr2;
			cond = run_parser(p, parse_exp);
			if(!cond) return NULL;
			lexer_next(&p->l, &tk);
			if(tk.type == TK_SEMICOLON || tk.type == TK_VSEMICOLON)
				lexer_next(&p->l, &tk);
			if(tk.type != TK_THEN)
				return lexer_unsee(&p->l, &tk), free_tree(cond), NULL;
			expr1 = run_parser(p, parse_exp);
			if(!expr1) return free_tree(cond), NULL;
			lexer_next(&p->l, &tk);
			if(tk.type == TK_SEMICOLON || tk.type == TK_VSEMICOLON)
				lexer_next(&p->l, &tk);
			if(tk.type != TK_ELSE) {
				lexer_unsee(&p->l, &tk);
				free_tree(cond);
				free_tree(expr1);
				return NULL;
			}
			expr2 = run_parser(p, parse_exp);
			if(!expr2)
				return free_tree(cond), free_tree(expr1), NULL;
			return new_tree_3(AST_IF, cond, expr1, expr2);
		}
	case TK_CASE:
		{
			tree *scrut, *branchlist;
			int virt;
			scrut = run_parser(p, parse_exp);
			if(!scrut) return NULL;
			lexer_next(&p->l, &tk);
			if(tk.type != TK_OF)
				return lexer_unsee(&p->l, &tk), free_tree(scrut), NULL;
			virt = lexer_next_open(&p->l, &tk);
			if(tk.type != TK_OPENBRACE && tk.type != TK_VOPENBRACE)
				return lexer_unsee(&p->l, &tk), free_tree(scrut), NULL;
			branchlist = run_parser(p, parse_alts);
			if(!branchlist) return free_tree(scrut), NULL;
			lexer_next_close(&p->l, &tk, virt);
			if(tk.type != (virt ? TK_VCLOSEBRACE : TK_CLOSEBRACE)) {
				lexer_unsee(&p->l, &tk);
				free_tree(scrut);
				free_tree(branchlist);
				return NULL;
			}
			return new_tree_2(AST_CASE, scrut, branchlist);
		}
	case TK_DO:
		{
			tree *stmts;
			int virt = lexer_next_open(&p->l, &tk);
			if(tk.type != TK_OPENBRACE && tk.type != TK_VOPENBRACE)
				return lexer_unsee(&p->l, &tk), NULL;
			stmts = run_parser(p, parse_stmts);
			if(!stmts) return NULL;
			lexer_next_close(&p->l, &tk, virt);
			if(tk.type != (virt ? TK_VCLOSEBRACE : TK_CLOSEBRACE))
				return lexer_unsee(&p->l, &tk), free_tree(stmts), NULL;
			return new_tree_1(AST_DO, stmts);
		}
	default:
		lexer_unsee(&p->l, &tk);
		return parse_fexp(p);
	}
}

static tree *parse_fexp(parser *p)
{
	tree *expr = run_parser(p, parse_aexp);
	if(!expr) return NULL;
	while(1) {
		tree *arg = try_parser(p, parse_aexp);
		if(!arg)
			return expr;
		expr = new_tree_2(AST_APPLY, expr, arg);
	}
}

static tree *parse_qop(parser *p)
{
	token tk;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_SYMBOL) {
		return new_tree_1(
			is_con_name(tk.u.name.name) ? AST_CON : AST_VAR,
			make_qualname(tk.u.name));
	} else if(tk.type == TK_BACKTICK) {
		tree *op;
		lexer_next(&p->l, &tk);
		if(tk.type != TK_NAME) return lexer_unsee(&p->l, &tk), NULL;
		op = new_tree_1(
			is_con_name(tk.u.name.name) ? AST_CON : AST_VAR,
			make_qualname(tk.u.name));
		lexer_next(&p->l, &tk);
		if(tk.type != TK_BACKTICK)
			return lexer_unsee(&p->l, &tk), free_tree(op), NULL;
	}
	lexer_unsee(&p->l, &tk);
	return NULL;
}

static tree *parse_paren_cont(parser *p)
{
	token tk;
	tree *op, *arg;
	op = try_parser(p, parse_qop);
	if(op) {
		arg = run_parser(p, parse_infixexp);
		if(!arg) return free_tree(op), NULL;
		lexer_next(&p->l, &tk);
		if(tk.type != TK_CLOSEPAREN)
			return lexer_unsee(&p->l, &tk), free_tree(arg), free_tree(op), NULL;
		return new_tree_2(AST_RSECTION, op, arg);
	} else {
		arg = run_parser(p, parse_infixexp);
		if(!arg) return NULL;
		op = run_parser(p, parse_qop);
		if(!op) return free_tree(arg), NULL;
		lexer_next(&p->l, &tk);
		if(tk.type != TK_CLOSEPAREN)
			return lexer_unsee(&p->l, &tk), free_tree(arg), free_tree(op), NULL;
		return new_tree_2(AST_LSECTION, op, arg);
	}
}

static tree *parse_paren_cont_right(parser *p)
{
	token tk;
	tree *op, *arg;
	op = run_parser(p, parse_qop);
	if(!op) return NULL;
	arg = run_parser(p, parse_infixexp);
	if(!arg) return free_tree(op), NULL;
	lexer_next(&p->l, &tk);
	if(tk.type != TK_CLOSEPAREN)
		return lexer_unsee(&p->l, &tk), free_tree(arg), free_tree(op), NULL;
	return new_tree_2(AST_RSECTION, op, arg);
}

static tree *parse_aexp(parser *p)
{
	token tk;
	tree *con, *expr;
	con = try_parser(p, parse_gcon);
	if(con) return new_tree_1(AST_CON, con);
	expr = try_parser(p, parse_qvar);
	if(expr) return expr;
	lexer_next(&p->l, &tk);
	switch(tk.type) {
	case TK_OPENPAREN:
		{
			tree *fun, *arg;
			unsigned long int *arity;
			struct parser_save s1, s2;
			/* Try parsing an 'exp' first, if we fail then we know that we also
			 * cannot parse an 'infixexp' at this position and so the section
			 * parser also shouldn't try to */
			save_parser(p, &s1);
			arg = run_parser(p, parse_exp);
			if(arg) {
				save_parser(p, &s2);
				restore_parser(p, &s1);
				expr = try_parser(p, parse_paren_cont);
				if(expr) {
					free_tree(arg);
					free_save(&s1);
					free_save(&s2);
					return expr;
				}
				free_save(&s1);
				restore_parser(p, &s2);
				free_save(&s2);
			} else {
				restore_parser(p, &s1);
				return free_save(&s1), run_parser(p, parse_paren_cont_right);
			}
			arity = make_int(0);
			fun = new_tree_1(AST_CON, new_tree_0_d(AST_TUPLE, arity));
			++*arity;
			fun = new_tree_2(AST_APPLY, fun, arg);
			while(1) {
				lexer_next(&p->l, &tk);
				if(tk.type == TK_CLOSEPAREN)
					break;
				else if(tk.type != TK_COMMA)
					return lexer_unsee(&p->l, &tk), free_tree(fun), NULL;
				arg = run_parser(p, parse_exp);
				if(!arg) return free_tree(fun), NULL;
				++*arity;
				fun = new_tree_2(AST_APPLY, fun, arg);
			}
			if(*arity == 1) {
				tree *expr = fun->children[1];
				fun->children[1] = NULL;
				free_tree(fun);
				return new_tree_1(AST_PARENS, expr);
			}
			return fun;
		}
	case TK_NUMBER:
		return new_tree_0_d(AST_NUMLIT, make_int(tk.u.number));
	case TK_CHAR:
		return new_tree_0_d(AST_CHARLIT, make_int(tk.u.number));
	case TK_STRING:
		return new_tree_0_d(AST_STRLIT, tk.u.string.chars);
	default:
		lexer_unsee(&p->l, &tk);
		return NULL;
	}
}

static tree *parse_bind_cont(parser *p)
{
	token tk;
	tree *pat = run_parser(p, parse_pat);
	if(!pat) return NULL;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_FROM)
		return pat;
	lexer_unsee(&p->l, &tk);
	return NULL;
}

static tree *parse_stmts(parser *p)
{
	return parse_semicolon_list(p, parse_stmt);
}

static tree *parse_stmt(parser *p)
{
	token tk;
	tree *expr, *pat;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_LET) {
		tree *decls;
		int virt = lexer_next_open(&p->l, &tk);
		if(tk.type != TK_OPENBRACE && tk.type != TK_VOPENBRACE)
			return lexer_unsee(&p->l, &tk), NULL;
		decls = run_parser(p, parse_decls);
		if(!decls) return NULL;
		lexer_next_close(&p->l, &tk, virt);
		if(tk.type != (virt ? TK_VCLOSEBRACE : TK_CLOSEBRACE))
			return lexer_unsee(&p->l, &tk), free_tree(decls), NULL;
		return new_tree_1(AST_STMT_LET, decls);
	}
	lexer_unsee(&p->l, &tk);
	pat = try_parser(p, parse_bind_cont);
	if(pat) {
		expr = run_parser(p, parse_exp);
		if(!expr) return free_tree(pat), NULL;
		return new_tree_2(AST_STMT_BIND, pat, expr);
	}
	expr = run_parser(p, parse_exp);
	if(!expr) return NULL;
	return new_tree_1(AST_STMT, expr);
}

static tree *parse_pat(parser *p)
{
	tree *con = try_parser(p, parse_gcon);
	if(con) {
		tree *pats = NULL;
		tree **pats_end = &pats;
		while(1) {
			tree *pat = try_parser(p, parse_apat);
			if(!pat)
				break;
			*pats_end = new_tree_2(AST_CONS, pat, NULL);
			pats_end = &(*pats_end)->children[1];
		}
		*pats_end = new_tree_0(AST_NIL);
		return new_tree_2(AST_PAT_CON, con, pats);
	} else {
		return parse_apat(p);
	}
}

static tree *parse_apat(parser *p)
{
	token tk;
	tree *con = try_parser(p, parse_gcon);
	if(con)
		return new_tree_2(AST_PAT_CON, con, new_tree_0(AST_NIL));
	lexer_next(&p->l, &tk);
	switch(tk.type) {
	case TK_OPENPAREN:
		{
			unsigned long int *arity = make_int(0);
			tree *pats = NULL;
			tree **pats_end = &pats;
			while(1) {
				tree *pat = try_parser(p, parse_pat);
				if(!pat) return free(arity), free_tree(pats), NULL;
				++*arity;
				*pats_end = new_tree_2(AST_CONS, pat, NULL);
				pats_end = &(*pats_end)->children[1];
				lexer_next(&p->l, &tk);
				if(tk.type == TK_CLOSEPAREN)
					break;
				else if(tk.type != TK_COMMA) {
					lexer_unsee(&p->l, &tk);
					free(arity);
					free_tree(pats);
					return NULL;
				}
			}
			if(*arity == 1) {
				tree *pat = pats->children[0];
				pats->children[0] = NULL;
				free(arity);
				free_tree(pats);
				return pat;
			}
			*pats_end = new_tree_0(AST_NIL);
			return new_tree_2(AST_PAT_CON,
				new_tree_0_d(AST_TUPLE, arity),
				pats);
		}
	case TK_NAME:
		if(!is_con_name(tk.u.name.name))
			return new_tree_1(AST_PAT_VAR, make_name(tk.u.name));
		else
			return lexer_unsee(&p->l, &tk), NULL;
	case TK_NUMBER:
		return new_tree_0_d(AST_PAT_NUMLIT, make_int(tk.u.number));
	case TK_CHAR:
		return new_tree_0_d(AST_PAT_CHARLIT, make_int(tk.u.number));
	case TK_STRING:
		return new_tree_0_d(AST_PAT_STRLIT, tk.u.string.chars);
	default:
		lexer_unsee(&p->l, &tk);
		return NULL;
	}
}

static tree *parse_alts(parser *p)
{
	return parse_semicolon_list(p, parse_alt);
}

static tree *parse_alt(parser *p)
{
	token tk;
	tree *pat = run_parser(p, parse_pat);
	if(!pat) return NULL;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_TO) {
		tree *expr = run_parser(p, parse_exp);
		if(!expr) return free_tree(pat), NULL;
		return new_tree_2(AST_BRANCH,
			pat,
			new_tree_2(AST_CONS,
				new_tree_2(AST_SWITCH,
					new_tree_0(AST_NIL),
					expr),
				new_tree_0(AST_NIL)));
	} else {
		tree *cases;
		lexer_unsee(&p->l, &tk);
		cases = run_parser(p, parse_gdpat);
		if(!cases) return free_tree(pat), NULL;
		return new_tree_2(AST_BRANCH, pat, cases);
	}
}

static tree *parse_gdpat(parser *p)
{
	token tk;
	tree *gd, *expr, *cases;
	gd = run_parser(p, parse_guards);
	if(!gd) return NULL;
	lexer_next(&p->l, &tk);
	if(tk.type != TK_TO) return lexer_unsee(&p->l, &tk), free_tree(gd), NULL;
	expr = run_parser(p, parse_exp);
	if(!expr) return free_tree(gd), NULL;
	cases = try_parser(p, parse_gdpat);
	if(!cases)
		cases = new_tree_0(AST_NIL);
	return new_tree_2(AST_CONS, new_tree_2(AST_SWITCH, gd, expr), cases);
}

static tree *parse_guards(parser *p)
{
	token tk;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_BAR) {
		tree *guards = NULL;
		tree **guards_end = &guards;
		while(1) {
			token tk;
			tree *guard = run_parser(p, parse_guard);
			if(!guard) return free_tree(guards), NULL;
			*guards_end = new_tree_2(AST_CONS, guard, NULL);
			guards_end = &(*guards_end)->children[1];
			lexer_next(&p->l, &tk);
			if(tk.type != TK_COMMA) {
				lexer_unsee(&p->l, &tk);
				break;
			}
		}
		*guards_end = new_tree_0(AST_NIL);
		return guards;
	}
	lexer_unsee(&p->l, &tk);
	return NULL;
}

static tree *parse_guard_cont(parser *p)
{
	token tk;
	tree *pat, *expr;
	pat = run_parser(p, parse_pat);
	if(!pat) return NULL;
	lexer_next(&p->l, &tk);
	if(tk.type != TK_FROM) return lexer_unsee(&p->l, &tk), free_tree(pat), NULL;
	expr = run_parser(p, parse_infixexp);
	if(!expr) return free_tree(pat), NULL;
	return new_tree_2(AST_GUARD_PAT, pat, expr);
}

static tree *parse_guard(parser *p)
{
	token tk;
	tree *guard, *expr;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_LET) {
		tree *decls;
		int virt = lexer_next_open(&p->l, &tk);
		if(tk.type != TK_OPENBRACE && tk.type != TK_VOPENBRACE)
			return lexer_unsee(&p->l, &tk), NULL;
		decls = run_parser(p, parse_decls);
		if(!decls) return NULL;
		lexer_next_close(&p->l, &tk, virt);
		if(tk.type != (virt ? TK_VCLOSEBRACE : TK_CLOSEBRACE))
			return lexer_unsee(&p->l, &tk), free_tree(decls), NULL;
		return new_tree_1(AST_GUARD_LET, decls);
	} else {
		lexer_unsee(&p->l, &tk);
		guard = try_parser(p, parse_guard_cont);
		if(guard) return guard;
		expr = run_parser(p, parse_infixexp);
		if(!expr) return NULL;
		return new_tree_1(AST_GUARD_BOOL, expr);
	}
}

tree *parse_topdecls(parser *p)
{
	return parse_semicolon_list(p, parse_topdecl);
}

static tree *parse_topdecl(parser *p)
{
	return parse_decl(p);
	/* TODO */
}

static tree *parse_decls(parser *p)
{
	return parse_semicolon_list(p, parse_decl);
}

static tree *parse_type_signature(parser *p)
{
	token tk;
	tree *var, *ty;
	var = run_parser(p, parse_var);
	if(!var) return NULL;
	lexer_next(&p->l, &tk);
	if(tk.type != TK_HASTYPE)
		return lexer_unsee(&p->l, &tk), free_tree(var), NULL;
	ty = run_parser(p, parse_type);
	if(!ty) return free_tree(var), NULL;
	return new_tree_2(AST_HAS_TYPE, var, ty);
}

static tree *parse_rhs(parser *p)
{
	token tk;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_EQUALS) {
		tree *expr = run_parser(p, parse_exp);
		if(!expr) return NULL;
		return new_tree_2(AST_CONS,
				new_tree_2(AST_SWITCH,
					new_tree_0(AST_NIL),
					expr),
				new_tree_0(AST_NIL));
	} else {
		lexer_unsee(&p->l, &tk);
		return parse_gdrhs(p);
	}
}

static tree *parse_gdrhs(parser *p)
{
	token tk;
	tree *gd, *expr, *cases;
	gd = run_parser(p, parse_guards);
	if(!gd) return NULL;
	lexer_next(&p->l, &tk);
	if(tk.type != TK_EQUALS)
		return lexer_unsee(&p->l, &tk), free_tree(gd), NULL;
	expr = run_parser(p, parse_exp);
	if(!expr) return free_tree(gd), NULL;
	cases = try_parser(p, parse_gdrhs);
	if(!cases)
		cases = new_tree_0(AST_NIL);
	return new_tree_2(AST_CONS, new_tree_2(AST_SWITCH, gd, expr), cases);
}

static tree *parse_decl(parser *p)
{
	token tk;
	lexer_next(&p->l, &tk);
	if(tk.type == TK_INFIX || tk.type == TK_INFIXL || tk.type == TK_INFIXR) {
		int decl_tag =
			tk.type == TK_INFIX ? AST_INFIX :
			tk.type == TK_INFIXL ? AST_INFIXL :
			AST_INFIXR;
		size_t fixity;
		lexer_next(&p->l, &tk);
		if(tk.type != TK_NUMBER)
			return lexer_unsee(&p->l, &tk), NULL;
		fixity = tk.u.number;
		lexer_next(&p->l, &tk);
		if(tk.type == TK_SYMBOL) {
			return new_tree_1_d(decl_tag,
						make_name(tk.u.name),
						make_int(fixity));
		} else if(tk.type == TK_BACKTICK) {
			tree *decl;
			lexer_next(&p->l, &tk);
			if(tk.type != TK_NAME) return lexer_unsee(&p->l, &tk), NULL;
			decl = new_tree_1_d(decl_tag,
				make_name(tk.u.name),
				make_int(fixity));
			lexer_next(&p->l, &tk);
			if(tk.type != TK_BACKTICK)
				return lexer_unsee(&p->l, &tk), free_tree(decl), NULL;
			return decl;
		}
	} else {
		tree *decl, *name, *pats, **pats_end, *rhs;
		lexer_unsee(&p->l, &tk);
		decl = try_parser(p, parse_type_signature);
		if(decl) return decl;
		name = run_parser(p, parse_var);
		if(!name) return NULL;
		pats = NULL;
		pats_end = &pats;
		while(1) {
			tree *pat = try_parser(p, parse_pat);
			if(!pat) break;
			*pats_end = new_tree_2(AST_CONS, pat, NULL);
			pats_end = &(*pats_end)->children[1];
		}
		*pats_end = new_tree_0(AST_NIL);
		rhs = run_parser(p, parse_rhs);
		if(!rhs) return free_tree(name), free_tree(pats), NULL;
		return new_tree_3(AST_BINDING, name, pats, rhs);
	}
	lexer_unsee(&p->l, &tk);
	return NULL;
}

void dump_ast(tree *t, size_t indent);

void dump_node(tree *t, size_t indent, char const *name, size_t children)
{
	size_t i;
	printf("%s\n", name);
	for(i = 0; i < children; ++i)
		dump_ast(t->children[i], indent + 1);
}

void dump_ast(tree *t, size_t indent)
{
	size_t i;
	for(i = 0; i < indent; ++i)
		printf(" ");
	ASSERT(t);
	switch(t->tag) {
	case AST_APPLY: dump_node(t, indent, "APPLY", 2); break;
	case AST_UOPERATOR: dump_node(t, indent, "UOPERATOR", 3); break;
	case AST_PARENS: dump_node(t, indent, "PARENS", 1); break;
	case AST_OPERATOR: dump_node(t, indent, "OPERATOR", 3); break;
	case AST_LSECTION: dump_node(t, indent, "LSECTION", 2); break;
	case AST_RSECTION: dump_node(t, indent, "RSECTION", 2); break;
	case AST_VAR: dump_node(t, indent, "VAR", 1); break;
	case AST_CON: dump_node(t, indent, "CON", 1); break;
	case AST_NUMLIT: printf("NUMLIT %lu\n", *(unsigned long int *)t->data); break;
	case AST_CHARLIT: printf("CHARLIT '%c'\n", (char)*(unsigned long int *)t->data); break;
	case AST_STRLIT: printf("STRLIT \"%s\"\n", (char *)t->data); break;
	case AST_CAST: dump_node(t, indent, "CAST", 2); break;
	case AST_LAMBDA: dump_node(t, indent, "LAMBDA", 2); break;
	case AST_IF: dump_node(t, indent, "IF", 3); break;
	case AST_CASE: dump_node(t, indent, "CASE", 2); break;
	case AST_LET: dump_node(t, indent, "LET", 2); break;
	case AST_DO: dump_node(t, indent, "DO", 1); break;
	case AST_PAT_CON: dump_node(t, indent, "PAT_CON", 2); break;
	case AST_PAT_AS: dump_node(t, indent, "PAT_AS", 2); break;
	case AST_PAT_VAR: dump_node(t, indent, "PAT_VAR", 1); break;
	case AST_PAT_NUMLIT: printf("PAT_NUMLIT %lu\n", *(unsigned long int *)t->data); break;
	case AST_PAT_CHARLIT: printf("PAT_CHARLIT '%c'\n", (char)*(unsigned long int *)t->data); break;
	case AST_PAT_STRLIT: printf("PAT_STRLIT \"%s\"\n", (char *)t->data); break;
	case AST_PAT_NONE: dump_node(t, indent, "PAT_NONE", 0); break;
	case AST_GUARD_PAT: dump_node(t, indent, "GUARD_PAT", 2); break;
	case AST_GUARD_LET: dump_node(t, indent, "GUARD_LET", 1); break;
	case AST_GUARD_BOOL: dump_node(t, indent, "GUARD_BOOL", 1); break;
	case AST_STMT: dump_node(t, indent, "STMT", 1); break;
	case AST_STMT_BIND: dump_node(t, indent, "STMT_BIND", 2); break;
	case AST_STMT_LET: dump_node(t, indent, "STMT_LET", 1); break;
	case AST_BINDING: dump_node(t, indent, "BINDING", 3); break;
	case AST_HAS_TYPE: dump_node(t, indent, "HAS_TYPE", 2); break;
	case AST_TYPE: dump_node(t, indent, "TYPE", 3); break;
	case AST_DATA: dump_node(t, indent, "DATA", 3); break;
	case AST_CLASS: dump_node(t, indent, "CLASS", 3); break;
	case AST_INSTANCE: dump_node(t, indent, "INSTANCE", 3); break;
	case AST_INFIX: printf("INFIX %lu\n", *(unsigned long int *)t->data); break;
	case AST_INFIXL: printf("INFIXL %lu\n", *(unsigned long int *)t->data); break;
	case AST_INFIXR: printf("INFIXR %lu\n", *(unsigned long int *)t->data); break;
	case AST_TYPE_APPLY: dump_node(t, indent, "TYPE_APPLY", 2); break;
	case AST_TYPE_ARROW: dump_node(t, indent, "TYPE_ARROW", 0); break;
	case AST_TYPE_CON: dump_node(t, indent, "TYPE_CON", 1); break;
	case AST_TYPE_VAR: dump_node(t, indent, "TYPE_VAR", 1); break;
	case AST_NAME: printf("NAME %s\n", *(char const **)t->data); break;
	case AST_QUALNAME: printf("QUALNAME %s.%s\n", ((qualname *)t->data)->qualifier, ((qualname *)t->data)->name); break;
	case AST_TUPLE: printf("TUPLE %lu\n", *(unsigned long int *)t->data); break;
	case AST_BRANCH: dump_node(t, indent, "BRANCH", 2); break;
	case AST_SWITCH: dump_node(t, indent, "SWITCH", 2); break;
	case AST_CONSTR: dump_node(t, indent, "CONSTR", 2); break;
	case AST_NIL: dump_node(t, indent, "NIL", 0); break;
	case AST_CONS: dump_node(t, indent, "CONS", 2); break;
	default: panic_fail("Unknown AST node: %d\n", t->tag); break;
	}
}
