#include "lazy_instance_engine.h"
#include "log.h"
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define LZTOK_LEX_LEN 64

typedef enum lztok_type_e lztok_type_e;

typedef struct lztok_s lztok_s;
typedef struct lztok_list_s lztok_list_s;

enum lztok_type_e {
	LZTYPE_NUM,
	LZTYPE_IDENT,
	LZTYPE_ADDOP,
	LZTYPE_MULOP,
  LZTYPE_LPAREN,
  LZTYPE_RPAREN,
	LZTYPE_EOF
};

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
static lztok_list_s lex(char *src);
static char *dupstr(char *str);

static double parse(lztok_list_s toklist, StrMap *symtable);

static double p_expression(lztok_s **t, StrMap *symtable);
static void p_expression_(lztok_s **t, double *exp, StrMap *symtable);
static double p_term(lztok_s **t, StrMap *symtable);
static void p_term_(lztok_s **t, double *term, StrMap *symtable);
static double p_factor(lztok_s **t, StrMap *symtable);
static int lookup_iterator_value(const char *key, StrMap *symtable);

double lazy_epxression_compute(StrMap *symtable, char *src) {
  char *nsrc = dupstr(src);
  lztok_list_s toklist = lex(nsrc);
  return parse(toklist, symtable);
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
          lz_add_tok(&toklist, bptr, LZTYPE_NUM);
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

char *dupstr(char *str) {
  size_t len = strlen(str);
  char *nstr = malloc(len);
  if (!nstr) {
    log_error("memory allocation error during string duplication");
    //trap
    return NULL;
  }
  strcpy(nstr, str);
  return nstr;
}

double parse(lztok_list_s toklist, StrMap *symtable) {
  lztok_s *t = toklist.head;
  p_expression(&t, symtable);
  if (t->type != LZTYPE_EOF) {
    log_error("Syntax Error: Expected end of expression, but got %s", t->lexeme);
  }
}

double p_expression(lztok_s **t, StrMap *symtable) {
	double val;
	lztok_s *op;

  switch ((*t)->type) {
    case LZTYPE_NUM:
    case LZTYPE_IDENT:
    case LZTYPE_LPAREN:
      val = p_term(t, symtable);
      p_expression_(t, &val, symtable);
      break;
    case LZTYPE_ADDOP:
			op = *t;
      *t = (*t)->next;
      val = p_expression(t, symtable);
			if (*op->lexeme == '-')
				val = -val;
      break;
    default:
      log_error("Syntax Error: expected number, +, -, '(', or variable reference, but got %s", 
					(*t)->lexeme);
			val = 0.0;
      break;
  }
	return val;
}

void p_expression_(lztok_s **t, double *exp, StrMap *symtable) {
	lztok_s *op;
	double term;

  if ((*t)->type == LZTYPE_ADDOP) {
		op = *t;
    *t = (*t)->next;
    term = p_term(t, symtable);
		if (*op->lexeme == '+')
			*exp += term;
		else
			*exp -= term;
    p_expression_(t, exp, symtable);
  }
}

double p_term(lztok_s **t, StrMap *symtable) {
	double val;

  switch ((*t)->type) {
    case LZTYPE_NUM:
    case LZTYPE_IDENT:
    case LZTYPE_LPAREN:
      val = p_factor(t, symtable);
      p_term_(t, &val, symtable);
      break;
    default:
      log_error("Syntax Error: expected number variable reference, or '(', but got %s", (*t)->lexeme);
			val = 0.0;
      break;
  }
	return val;
}

void p_term_(lztok_s **t, double *term, StrMap *symtable) {
	double factor;
	lztok_s *op;
  if ((*t)->type == LZTYPE_MULOP) {
		op = *t;
    *t = (*t)->next;
    factor = p_factor(t, symtable);
		if (*op->lexeme == '*')
			*term = *term * factor;
		else
			*term = *term / factor;
    p_term_(t, term, symtable);
  }
}

double p_factor(lztok_s **t, StrMap *symtable) {
	double value;
  switch ((*t)->type) {
    case LZTYPE_NUM:
			value = atof((*t)->lexeme);
      *t = (*t)->next;
			break;
    case LZTYPE_IDENT:
			value = (double)lookup_iterator_value((*t)->lexeme, symtable);
      *t = (*t)->next;
      break;
    case LZTYPE_LPAREN:
      *t = (*t)->next;
      value = p_expression(t, symtable);
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

int lookup_iterator_value(const char *key, StrMap *symtable) {
	void *result = bob_str_map_get(symtable, key);	
	intptr_t intptr = (intptr_t)result;
	return intptr;
}


