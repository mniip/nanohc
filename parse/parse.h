#ifndef PARSER_H_
#define PARSER_H_

#include "lex.h"

/* Tags for the AST (in parentheses the children values are listed) */

enum ast_tag {
/* Expressions: */
	/* Function application (function, arg) */
	AST_APPLY,
	/* Not-yet-associated operator (operator, arg1, arg2) */
	AST_UOPERATOR,
	/* Parenthesized expression (expr) */
	AST_PARENS,
	/* Operator application (operator, arg1, arg2) */
	AST_OPERATOR,
	/* Operator sections (operator, arg) */
	AST_LSECTION,
	AST_RSECTION,
	/* Variable (qualname) */
	AST_VAR,
	/* Constructor (qualname) */
	AST_CON,
	/* Numeric literal (data is a pointer to an integer) */
	AST_NUMLIT,
	/* Char literal (data is a pointer to an integer value of the char) */
	AST_CHARLIT,
	/* String literal (data is a pointer to the string) */
	AST_STRLIT,
	/* Type specification (expr, type) */
	AST_CAST,
	/* Lambda (patlist, expr) */
	AST_LAMBDA,
	/* If (cond, expr1, expr2) */
	AST_IF,
	/* Case (scrutinee, branchlist) */
	AST_CASE,
	/* Let (decllist, expr) */
	AST_LET,
	/* Do (stmtlist) */
	AST_DO,

/* Patterns: */
	/* Constructor pattern (qualname, patlist) */
	AST_PAT_CON,
	/* As-pattern (name, pat) */
	AST_PAT_AS,
	/* Variable pattern (name) */
	AST_PAT_VAR,
	/* Numeric literal pattern (data is a pointer to an integer) */
	AST_PAT_NUMLIT,
	/* Char literal pattern (data is a pointer to an integer value of the char)
	 */
	AST_PAT_CHARLIT,
	/* Sgtring literal pattern (data is a pointer to the string) */
	AST_PAT_STRLIT,
	/* Ignored pattern */
	AST_PAT_NONE,

/* Guards: */
	/* Pattern guard (pat, expr) */
	AST_GUARD_PAT,
	/* Let-guard (decllist) */
	AST_GUARD_LET,
	/* Boolean guard (expr) */
	AST_GUARD_BOOL,

/* Statements: */
	/* Do-statement (expr) */
	AST_STMT,
	/* Do-statement with bind (pat, expr) */
	AST_STMT_BIND,
	/* Do-statement let (decllist) */
	AST_STMT_LET,

/* Declarations: */
	/* A value binding (name, patlist, caselist) */
	AST_BINDING,
	/* A type signature (name, type) */
	AST_HAS_TYPE,
	/* A type synonym declaration (name, namelist, type) */
	AST_TYPE,
	/* A datatype declaration (name, namelist, constrlist) */
	AST_DATA,
	/* A class declaration (name, namelist, decllist) */
	AST_CLASS,
	/* An instance declaration (qualname, typelist, decllist) */
	AST_INSTANCE,
	/* A fixity declaration (qualname) (data is pointer to an integer) */
	AST_INFIX,
	AST_INFIXL,
	AST_INFIXR,

/* Types: */
	/* Type application (fun, arg) */
	AST_TYPE_APPLY,
	/* Arrow type () */
	AST_TYPE_ARROW,
	/* Type constructor (qualname) */
	AST_TYPE_CON,
	/* Type variable (name) */
	AST_TYPE_VAR,

/* Misc: */
	/* An identifier name (data is a pointer to an interned pointer to a name */
	AST_NAME,
	/* A possibly qualified name (data pointer is a pointer to a qualname) */
	AST_QUALNAME,
	/* Tuple (data is pointer to integer which is arity) */
	AST_TUPLE,
	/* Case branch (pat, caselist) */
	AST_BRANCH,
	/* A guarded expression (guardlist, expr) */
	AST_SWITCH,
	/* A datatype constructor declaration (name, typelist) */
	AST_CONSTR,


/* Lists: */
	/* () */
	AST_NIL,
	/* (head, tail) */
	AST_CONS
};

#define MAX_DEPTH 0x1000

typedef struct parser {
	lexer l;
	size_t depth;
} parser;

extern void parser_new(parser *, char const *data);
extern void parser_eof(parser *);
extern void parser_free(parser *);

extern tree *parse_topdecls(parser *);
extern tree *parse_type(parser *);
extern tree *parse_exp(parser *);

extern void dump_ast(tree *, size_t);

#endif
