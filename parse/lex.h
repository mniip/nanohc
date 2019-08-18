#ifndef LEX_H_
#define LEX_H_

#include <stddef.h>

/* An operator symbol, optionally with a qualifier */
#define TK_SYMBOL      0x00
/* A regular identifier (or a constructor), optionally with a qualifier */
#define TK_NAME        0x01
/* A numeric literal */
#define TK_NUMBER      0x02
/* A character literal */
#define TK_CHAR        0x03
/* A string literal */
#define TK_STRING      0x04

/* Keywords */
#define TK_CASE        0x05
#define TK_CLASS       0x06
#define TK_DATA        0x07
#define TK_DERIVING    0x08
#define TK_DO          0x09
#define TK_ELSE        0x0A
#define TK_IF          0x0B
#define TK_IMPORT      0x0C
#define TK_IN          0x0D
#define TK_INFIX       0x0E
#define TK_INFIXL      0x0F
#define TK_INFIXR      0x10
#define TK_INSTANCE    0x11
#define TK_LET         0x12
#define TK_MODULE      0x13
#define TK_NEWTYPE     0x14
#define TK_OF          0x15
#define TK_THEN        0x16
#define TK_TYPE        0x17
#define TK_WHERE       0x18

/* { */
#define TK_OPENBRACE   0x19
/* } */
#define TK_CLOSEBRACE  0x1A
/* ; */
#define TK_SEMICOLON   0x1B
/* A virtual { inferred from layout */
#define TK_VOPENBRACE  0x1C
/* A virtual } inferred from layout */
#define TK_VCLOSEBRACE 0x1D
/* A virtual ; inferred from layout */
#define TK_VSEMICOLON  0x1E
/* , */
#define TK_COMMA       0x1F
/* .. */
#define TK_RANGE       0x20
/* :: */
#define TK_HASTYPE     0x21
/* = */
#define TK_EQUALS      0x22
/* \ */
#define TK_LAMBDA      0x23
/* | */
#define TK_BAR         0x24
/* <- */
#define TK_FROM        0x25
/* -> */
#define TK_TO          0x26
/* @ */
#define TK_AT          0x27
/* => */
#define TK_CONTEXT     0x28
/* End of file or invalid token */
#define TK_EOF         0xFF

typedef struct token {
	int type;
	/* Offset from the beginning of the lexed string */
	size_t pos;
	/* Line number */
	size_t line;
	/* Offset from the beginning of the line */
	size_t col;
	union {
		/* TK_SYMBOL or TK_NAME */
		struct {
			/* An optional interned qualifier */
			char const *qualifier;
			/* The symbol/identifier name */
			char const *name;
		} name;
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
	/* Indentation on the current line */
	size_t current_indent;
	/* Current layout state: a list of indent depths */
	size_t *indents;
	size_t num_indents;
	/* Populated when expect a brace but the next token isn't one */
	struct token next;
} lexer;

extern void lexer_new(lexer *l, char const *data);
extern void lexer_free(lexer *l);

/* Read the next token from the input. If we begin a new line whose indentation
 * is the same as the last layout state, return a virtual semicolon. If less,
 * return a virtual closing brace. */
extern void lexer_next(lexer *l, token *t);

/* Return a token (presumably one that was just lexed) so that it is returned on
 * the next "lexer_next" invocation. Not possible after a "lexer_next_brace". */
extern void lexer_unsee(lexer *l, token *t);

/* Expect a brace next, if there isn't one generate a virtual opening brace and
 * push the current indentation into the layout state. */
extern void lexer_next_brace(lexer *l, token *t);

/* Pop an indentation from the layout state. */
extern void lexer_outdent(lexer *l);

#endif
