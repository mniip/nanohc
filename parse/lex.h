#ifndef LEX_H_
#define LEX_H_

#include <stddef.h>

enum token_type {
	/* An operator symbol, optionally with a qualifier */
	TK_SYMBOL,
	/* A regular identifier (or a constructor), optionally with a qualifier */
	TK_NAME,
	/* A numeric literal */
	TK_NUMBER,
	/* A character literal */
	TK_CHAR,
	/* A string literal */
	TK_STRING,

	/* Keywords */
	TK_CASE,
	TK_CLASS,
	TK_DATA,
	TK_DERIVING,
	TK_DO,
	TK_ELSE,
	TK_IF,
	TK_IMPORT,
	TK_IN,
	TK_INFIX,
	TK_INFIXL,
	TK_INFIXR,
	TK_INSTANCE,
	TK_LET,
	TK_MODULE,
	TK_NEWTYPE,
	TK_OF,
	TK_THEN,
	TK_TYPE,
	TK_WHERE,

	/* { */
	TK_OPENBRACE,
	/* } */
	TK_CLOSEBRACE,
	/* ; */
	TK_SEMICOLON,
	/* A virtual { inferred from layout */
	TK_VOPENBRACE,
	/* A virtual } inferred from layout */
	TK_VCLOSEBRACE,
	/* A virtual ; inferred from layout */
	TK_VSEMICOLON,
	/* ( */
	TK_OPENPAREN,
	/* ) */
	TK_CLOSEPAREN,
	/* [ */
	TK_OPENBRACKET,
	/* ] */
	TK_CLOSEBRACKET,
	/* , */
	TK_COMMA,
	/* .. */
	TK_RANGE,
	/* :: */
	TK_HASTYPE,
	/* = */
	TK_EQUALS,
	/* \ */
	TK_LAMBDA,
	/* | */
	TK_BAR,
	/* <- */
	TK_FROM,
	/* -> */
	TK_TO,
	/* @ */
	TK_AT,
	/* => */
	TK_CONTEXT,
	/* ` */
	TK_BACKTICK,
	/* End of file or invalid token */
	TK_EOF = 0xFF
};

typedef struct qualname {
	/* An optional interned qualifier */
	char const *qualifier;
	/* The symbol/identifier name */
	char const *name;
} qualname;

typedef struct token {
	enum token_type type;
	/* Offset from the beginning of the lexed string */
	size_t pos;
	/* Line number */
	size_t line;
	/* Offset from the beginning of the line */
	size_t col;
	/* Indentation of token */
	size_t indent;
	union {
		/* TK_SYMBOL or TK_NAME */
		qualname name;
		/* An allocated string, for TK_STRING, to be freed by the user */
		struct {
			char *chars;
			size_t num_chars;
		} string;
		/* A numeric value, for TK_NUMBER */
		unsigned long int number;
	} u;
} token;

typedef struct lexer {
	/* The NULL-terminated string we're parsing */
	char const *data;
	size_t pos;
	size_t line;
	size_t line_start;
	/* Have we seen any non-whitespace chars on this line */
	int line_started;
	/* Difference between the current indentation and the current position in
	 * the line */
	size_t indent_adj;
	/* Current layout state: a list of indent depths */
	size_t *indents;
	size_t num_indents;
	/* Populated when expect a brace but the next token isn't one */
	struct token next;
} lexer;

extern void lexer_new(lexer *, char const *data);
extern void lexer_free(lexer *);
extern void lexer_copy(lexer *, lexer const *);

/* Read the next token from the input. If we begin a new line whose indentation
 * is the same as the last layout state, return a virtual semicolon. If less,
 * return a virtual closing brace. */
extern void lexer_next(lexer *, token *);

/* Return a token (presumably one that was just lexed) so that it is returned on
 * the next "lexer_next" invocation. Not possible after "lexer_next_open" has
 * returned a virtual opening brace. */
extern void lexer_unsee(lexer *, token const *);

/* Expect an opening brace next, if there isn't one generate a virtual opening
 * brace and push the current indentation into the layout state. Returns whether
 * the brace was virtual. */
extern int lexer_next_open(lexer *, token *);

/* If the "virt" parameter is true then return a virtual brace and pop the
 * current indentation from the layout state. Otherwise return the next token */
extern void lexer_next_close(lexer *, token *, int virt);

#endif
