#include "lazy_instance_engine.h"
#include "common/log.h"
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define LZTOK_LEX_LEN 64
typedef struct lztok_s lztok_s;
typedef struct lztok_list_s lztok_list_s;

typedef enum  {
	LZTYPE_NUM,
	LZTYPE_IDENT,
	LZTYPE_ADDOP,
	LZTYPE_MULOP,
	LZTYPE_LPAREN,
	LZTYPE_RPAREN,
	LZTYPE_EOF
} lztok_type_e;

struct lztok_s {
	lztok_type_e type;
	char lexeme[LZTOK_LEX_LEN];
	lztok_s *next;
};

struct lztok_list_s {
	lztok_s *head;
	lztok_s *tail;
};

static void lz_add_tok(lztok_list_s *list, char *lexeme, lztok_type_e type);
static void lz_toklist_free(lztok_list_s *list);
static lztok_list_s lex(char *src);

static float parse(lztok_list_s toklist, Range *range);

static float p_expression(lztok_s **t, Range *range);
static void p_expression_(lztok_s **t, float *exp, Range *range);
static float p_term(lztok_s **t, Range *range);
static void p_term_(lztok_s **t, float *term, Range *range);
static float p_factor(lztok_s **t, Range *range);
static int lookup_iterator_value(const char var, Range *range);

float lazy_epxression_compute(Range *range, char *src) {
	char *nsrc = bob_dup_str(src);
	lztok_list_s toklist = lex(nsrc);
	float result = parse(toklist, range);
	lz_toklist_free(&toklist); 
  free(nsrc);
	return result;
}

lztok_list_s lex(char *src) {
	char bck;
	char *fptr = src, *bptr;

	lztok_list_s toklist = {NULL, NULL};

	while (*fptr) {
		switch (*fptr) {
			case ' ':
			case '\t':
			case '\v':
			case '\n':
			case '\r':
				fptr++;
				break;
			case '+':
				lz_add_tok(&toklist, "+", LZTYPE_ADDOP);
				fptr++;
				break;
			case '-':
				lz_add_tok(&toklist, "-", LZTYPE_ADDOP);
				fptr++;
				break;
			case '*':
				lz_add_tok(&toklist, "*", LZTYPE_MULOP);
				fptr++;
				break;
			case '/':
				lz_add_tok(&toklist, "/", LZTYPE_MULOP);
				fptr++;
				break;
			case '(':
				lz_add_tok(&toklist, "(", LZTYPE_LPAREN);
				fptr++;
				break;
			case ')':
				lz_add_tok(&toklist, ")", LZTYPE_RPAREN);
				fptr++;
				break;
			default:
				if (isdigit(*fptr)) {
					bptr = fptr;
					while (isdigit(*++fptr));
					if (*fptr == '.') {
						while (isdigit(*++fptr));
					}
					bck = *fptr;
					*fptr = '\0';
					lz_add_tok(&toklist, bptr, LZTYPE_NUM);
					*fptr = bck;
				} 
				else if (isalpha(*fptr)) {
					bptr = fptr;
					while (isalpha(*++fptr));
					bck = *fptr;
					*fptr = '\0';
					lz_add_tok(&toklist, bptr, LZTYPE_IDENT);
					*fptr = bck;
				}
				else {
					log_error("lexical error: unknown character: %c", *fptr);
					fptr++;
				}
				break;
		}
	}
	lz_add_tok(&toklist, "$", LZTYPE_EOF);
	return toklist;
}

void lz_add_tok(lztok_list_s *list, char *lexeme, lztok_type_e type) {
	lztok_s *ntok = malloc(sizeof *ntok);
	if (!ntok) {
		log_error("failed to allocate memory for new lz token");
		//TODO: trap
	}
	ntok->type = type;
	ntok->next = NULL;
	strcpy(ntok->lexeme, lexeme);
	if (!list->head) {
		list->head = ntok;
	} else {
		list->tail->next = ntok;
	}
	list->tail = ntok;
}

void lz_toklist_free(lztok_list_s *list) {
	lztok_s *t = list->head, *bck;
	while (t) {
		bck = t;	
		t = t->next;
		free(bck);
	}
}

float parse(lztok_list_s toklist, Range *range) {
	lztok_s *t = toklist.head;
	float result = p_expression(&t, range);
	if (t->type != LZTYPE_EOF) {
		log_error("Syntax Error: Expected end of expression, but got %s", t->lexeme);
	}
	return result;
}

float p_expression(lztok_s **t, Range *range) {
	float val;
	lztok_s *op;

	switch ((*t)->type) {
		case LZTYPE_NUM:
		case LZTYPE_IDENT:
		case LZTYPE_LPAREN:
			val = p_term(t, range);
			p_expression_(t, &val, range);
			break;
		case LZTYPE_ADDOP:
			op = *t;
			*t = (*t)->next;
			val = p_expression(t, range);
			if (*op->lexeme == '-')
				val = -val;
			break;
		default:
			log_error(
					"Syntax Error: expected number, +, -, '(', or variable reference, but got %s", 
					(*t)->lexeme);
			val = 0.0;
			break;
	}
	return val;
}

void p_expression_(lztok_s **t, float *exp, Range *range) {
	lztok_s *op;
	float term;

	if ((*t)->type == LZTYPE_ADDOP) {
		op = *t;
		*t = (*t)->next;
		term = p_term(t, range);
		if (*op->lexeme == '+')
			*exp += term;
		else
			*exp -= term;
		p_expression_(t, exp, range);
	}
}

float p_term(lztok_s **t, Range *range) {
	float val;

	switch ((*t)->type) {
		case LZTYPE_NUM:
		case LZTYPE_IDENT:
		case LZTYPE_LPAREN:
			val = p_factor(t, range);
			p_term_(t, &val, range);
			break;
		default:
			log_error("Syntax Error: expected number variable reference, or '(', but got %s", 
					(*t)->lexeme);
			val = 0.0;
			break;
	}
	return val;
}

void p_term_(lztok_s **t, float *term, Range *range) {
	float factor;
	lztok_s *op;
	if ((*t)->type == LZTYPE_MULOP) {
		op = *t;
		*t = (*t)->next;
		factor = p_factor(t, range);
		if (*op->lexeme == '*')
			*term = *term * factor;
		else
			*term = *term / factor;
		p_term_(t, term, range);
	}
}

float p_factor(lztok_s **t, Range *range) {
	float value;
	switch ((*t)->type) {
		case LZTYPE_NUM:
			value = atof((*t)->lexeme);
			*t = (*t)->next;
			break;
		case LZTYPE_IDENT:
			value = (float)lookup_iterator_value(*(*t)->lexeme, range);
			*t = (*t)->next;
			break;
		case LZTYPE_LPAREN:
			*t = (*t)->next;
			value = p_expression(t, range);
			if ((*t)->type == LZTYPE_RPAREN) {
				*t = (*t)->next;
			} else {
				log_error("Syntax Error: expected ')' but got %s", (*t)->lexeme);
			}
			break;
		default:
			log_error("Syntax Error: expected number, variable reference, or '(' but got %s", 
					(*t)->lexeme);
			value = 0.0;
			break;
	}
	return value;
}

int lookup_iterator_value(const char var, Range *range) {
	while (range && range->var != var) range = range->parent;
	if (!range) {
		log_error("access to undeclared iteraor variable within range %c", var);
		return -1;
	}
	return range->currval;
}


