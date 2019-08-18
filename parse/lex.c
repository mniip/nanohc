#include <stdio.h>
#include <string.h>

#include "data.h"
#include "lex.h"
#include "util.h"

void lexer_new(lexer *l, char const *data)
{
	l->data = data;
	l->pos = 0;
	l->line_started = 0;
	l->current_indent = 0;
	l->indents = NULL;
	l->num_indents = 0;
	l->next.type = TK_EOF;
}

void lexer_free(lexer *l)
{
	free_array(size_t, &l->indents, &l->num_indents);
	if(l->next.type == TK_STRING)
		free_array(char, &l->next.u.string.chars, &l->next.u.string.num_chars);
}

void lexer_unsee(lexer *l, token *t)
{
	ASSERT(l->next.type == TK_EOF);
	memcpy(&l->next, t, sizeof l->next);
}

void lexer_outdent(lexer *l)
{
	ASSERT(l->num_indents);
	shrink_array(size_t, l->indents, &l->num_indents);
}

static void set_token(lexer *l, token *t, int type)
{
	t->type = type;
	t->pos = l->pos;
	t->line = l->line;
	t->col = l->pos - l->line_start;
}

void lexer_next_brace(lexer *l, token *t)
{
	token tmp;
	lexer_next(l, &tmp);
	if(tmp.type == TK_OPENBRACE) {
		memcpy(t, &tmp, sizeof *t);
		return;
	}
	grow_array(size_t, l->indents, &l->num_indents);
	l->indents[l->num_indents - 1] = l->current_indent;
	lexer_unsee(l, t);
	set_token(l, t, TK_VOPENBRACE);
}

static int is_symbol(char c)
{
	return !!strchr("!#$%&*+./<=>?@\\^|-~:", c);
}

static int is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_number(char c)
{
	return c >= '0' && c <= '9';
}

static int is_alnum(char c)
{
	return is_alpha(c) || is_number(c);
}

static int from_escape(char c)
{
	switch(c) {
	case 'a': return '\a';
	case 'b': return '\b';
	case 'f': return '\f';
	case 'n': return '\n';
	case 'r': return '\r';
	case 't': return '\t';
	case 'v': return '\v';
	case '\\': return '\\';
	case '"': return '"';
	case '\'': return '\'';
	default: panic("Unknown escape: \\%c", c);
		return 0;
	}
}

static int from_hexdigit(char c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	panic("Not a hex digit: %c", c);
	return 0;
}

/* TODO: replace asserts with something more informative */
void lexer_next(lexer *l, token *t)
{
	/* Skip whitespace */
	int skipping = 1;
	while(skipping) {
		switch(l->data[l->pos]) {
		case '\r':
		case '\n':
		case '\f':
			if(l->data[l->pos] == '\r' && l->data[l->pos + 1] == '\n')
				l->pos += 2;
			else
				l->pos += 1;
			++l->line;
			l->line_start = l->pos;
			l->line_started = 0;
			l->current_indent = 0;
			break;
		case '\t':
			++l->pos;
			l->current_indent = (l->current_indent | 7) + 1;
			break;
		case '\v': /* Empirically tested on GHC */
		case ' ':
			++l->pos;
			l->current_indent += 1;
			break;
		case '-':
			/* Line comment */
			if(l->data[l->pos + 1] == '-') {
				size_t offt = 0;
				while(l->data[offt] == '-')
					++offt;
				/* Actually just an operator beginning with -- */
				if(is_symbol(l->data[offt]))
					skipping = 0;
				else {
					l->pos = offt;
					while(l->data[l->pos]
						&& l->data[l->pos] != '\n' && l->data[l->pos] != '\r')
						++l->pos;
				}
			} else
				skipping = 0;
			break;
		case '{':
			/* Block comment */
			if(l->data[l->pos + 1] == '-') {
				size_t depth = 1;
				l->pos += 2;
				while(depth)
					if(l->data[l->pos] == '{' && l->data[l->pos + 1] == '-') {
						++depth;
						l->pos += 2;
					} else
					if(l->data[l->pos] == '-' && l->data[l->pos + 1] == '}') {
						--depth;
						l->pos += 2;
					} else {
						ASSERT(l->data[l->pos]);
						++l->pos;
					}
			} else
				skipping = 0;
			break;
		default:
			skipping = 0;
			break;
		}
	}
	/* Virtual tokens from layout rules */
	if(!l->line_started && l->num_indents) {
		if(l->current_indent == l->indents[l->num_indents - 1]) {
			set_token(l, t, TK_VSEMICOLON);
			return;
		} else if(l->current_indent < l->indents[l->num_indents - 1]) {
			shrink_array(size_t, l->indents, &l->num_indents);
			set_token(l, t, TK_VCLOSEBRACE);
			return;
		}
	}
	l->line_started = 1;
	switch(l->data[l->pos]) {
	case 0:
		set_token(l, t, TK_EOF);
		return;
	case '{':
		set_token(l, t, TK_OPENBRACE);
		++l->pos;
		return;
	case '}':
		set_token(l, t, TK_CLOSEBRACE);
		++l->pos;
		return;
	case ';':
		set_token(l, t, TK_SEMICOLON);
		++l->pos;
		return;
	case ',':
		set_token(l, t, TK_COMMA);
		++l->pos;
		return;
	case '\'':
		set_token(l, t, TK_CHAR);
		++l->pos;
		if(l->data[l->pos] == '\\') {
			++l->pos;
			if(l->data[l->pos] == 'x') {
				++l->pos;
				t->u.number = 16 * from_hexdigit(l->data[l->pos + 1])
					+ from_hexdigit(l->data[l->pos + 2]);
				l->pos += 2;
			} else {
				t->u.number = from_escape(l->data[l->pos]);
				++l->pos;
			}
		} else {
			t->u.number = l->data[l->pos];
			++l->pos;
		}
		ASSERT(l->data[l->pos] == '\'');
		++l->pos;
		return;
	case '"': {
			char *chars = NULL;
			size_t num_chars = 0;
			set_token(l, t, TK_STRING);
			++l->pos;
			while(l->data[l->pos] != '"') {
				if(l->data[l->pos] == '\\') {
					++l->pos;
					if(l->data[l->pos] == 'x') {
						++l->pos;
						grow_array(char, &chars, &num_chars);
						chars[num_chars - 1] =
							16 * from_hexdigit(l->data[l->pos + 1])
							+ from_hexdigit(l->data[l->pos + 2]);
						l->pos += 2;
					} else {
						grow_array(char, &chars, &num_chars);
						chars[num_chars - 1] = from_escape(l->data[l->pos]);
						++l->pos;
					}
				} else {
					ASSERT(l->data[l->pos]);
					grow_array(char, &chars, &num_chars);
					chars[num_chars - 1] = l->data[l->pos];
					++l->pos;
				}
			}
		}
	}
	if(is_symbol(l->data[l->pos]))
	{
		size_t offt = l->pos;
		while(is_symbol(l->data[offt]))
			++offt;
		if(offt == l->pos + 2) {
			if(l->data[l->pos] == '.' && l->data[l->pos + 1] == '.') {
				set_token(l, t, TK_RANGE);
				l->pos = offt;
				return;
			} else if(l->data[l->pos] == ':' && l->data[l->pos + 1] == ':') {
				set_token(l, t, TK_HASTYPE);
				l->pos = offt;
				return;
			} else if(l->data[l->pos] == '<' && l->data[l->pos + 1] == '-') {
				set_token(l, t, TK_FROM);
				l->pos = offt;
				return;
			} else if(l->data[l->pos] == '-' && l->data[l->pos + 1] == '>') {
				set_token(l, t, TK_TO);
				l->pos = offt;
				return;
			} else if(l->data[l->pos] == '=' && l->data[l->pos + 1] == '>') {
				set_token(l, t, TK_CONTEXT);
				l->pos = offt;
				return;
			}
		} else if(offt == l->pos + 1) {
			if(l->data[l->pos] == '=') {
				set_token(l, t, TK_EQUALS);
				l->pos = offt;
				return;
			} else if(l->data[l->pos] == '\\') {
				set_token(l, t, TK_EQUALS);
				l->pos = offt;
				return;
			} else if(l->data[l->pos] == '|') {
				set_token(l, t, TK_BAR);
				l->pos = offt;
				return;
			} else if(l->data[l->pos] == '@') {
				set_token(l, t, TK_AT);
				l->pos = offt;
				return;
			}
		}
		set_token(l, t, TK_SYMBOL);
		t->u.name.qualifier = NULL;
		t->u.name.name = intern_sz(&l->data[l->pos], offt - l->pos);
		l->pos = offt;
		return;
	} else if(is_alpha(l->data[l->pos])) {
		size_t offt = l->pos;
		size_t name_offt = l->pos;
		while(is_alnum(l->data[offt])) {
			++offt;
			if(l->data[offt] == '.' && is_alpha(l->data[offt + 1])) {
				++offt;
				name_offt = offt;
			}
		}
		if(l->data[offt] == '.' && is_symbol(l->data[offt + 1])) {
			++offt;
			name_offt = offt;
			while(is_symbol(l->data[offt]))
				++offt;
			set_token(l, t, TK_SYMBOL);
			t->u.name.qualifier =
				intern_sz(&l->data[l->pos], name_offt - l->pos - 1);
			t->u.name.name = intern_sz(&l->data[l->pos], offt - name_offt);
			l->pos = offt;
			return;
		}
		set_token(l, t, TK_NAME);
		t->u.name.qualifier = name_offt == offt ? NULL :
			intern_sz(&l->data[l->pos], name_offt - l->pos - 1);
		t->u.name.name = intern_sz(&l->data[l->pos], offt - name_offt);
		l->pos = offt;
		return;
	} else if(is_number(l->data[l->pos])) {
		int len = 0;
		unsigned long int number = 0;
		ASSERT(2 == sscanf(l->data, "%li%n", (long int *)&number, &len));
		set_token(l, t, TK_NUMBER);
		t->u.number = number;
		l->pos += len;
		return;
	}
	panic("Invalid symbol: %c", l->data[l->pos]);
}
