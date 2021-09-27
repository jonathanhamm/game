#include "lazy_instance_engine.h"
#include "log.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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

static void parse(lztok_list_s toklist);

static void p_expression(lztok_s **t);
static void p_expression_(lztok_s **t);
static void p_term(lztok_s **t);
static void p_term_(lztok_s **t);
static void p_factor(lztok_s **t);

/*
 *
  
 <expression> ->
	<simple_expression> <expression'>
<expression'> ->
	relop <simple_expression>
	|
	ε	

<simple_expression> ->
	<sign> <simple_expression>
	|
	<term> <simple_expression'>
    
<simple_expression'> ->
	addop <term> <simple_expression'>
	|
	ε

<term> ->
	<factor> <term'>

<term'> ->
	mulop <factor> <term'>
	|
	ε

<factor> ->
	|
	num
  |
  ident
	|
    |
    \( <expression> \)

 
 */

double lazy_epxression_compute(char *src, double val) {
  char *nsrc = dupstr(src);
  lztok_list_s toklist = lex(nsrc);
  parse(toklist);
	return 0;
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

void parse(lztok_list_s toklist) {
  lztok_s *t = toklist.head;
  p_expression(&t);
  if (t->type != LZTYPE_EOF) {
    log_error("Syntax Error: Expected end of expression, but got %s", t->lexeme);
  }
}

void p_expression(lztok_s **t) {
  switch ((*t)->type) {
    case LZTYPE_NUM:
    case LZTYPE_IDENT:
    case LZTYPE_LPAREN:
      p_term(t);
      p_expression_(t);
      break;
    case LZTYPE_ADDOP:
      *t = (*t)->next;
      p_expression(t);
      break;
    default:
      log_error("Syntax Error: expected number, +, -, '(', or variable reference, but got %s", (*t)->lexeme);
      break;
  }
}

void p_expression_(lztok_s **t) {
  if ((*t)->type == LZTYPE_ADDOP) {
    *t = (*t)->next;
    p_term(t);
    p_expression_(t);
  }
}

void  p_term(lztok_s **t) {
  switch ((*t)->type) {
    case LZTYPE_NUM:
    case LZTYPE_IDENT:
    case LZTYPE_LPAREN:
      p_factor(t);
      p_term_(t);
      break;
    default:
      log_error("Syntax Error: expected number variable reference, or '(', but got %s", (*t)->lexeme);
      break;
  }
}

void p_term_(lztok_s **t) {
  if ((*t)->type == LZTYPE_MULOP) {
    *t = (*t)->next;
    p_factor(t);
    p_term_(t);
  }
}

void p_factor(lztok_s **t) {

  switch ((*t)->type) {
    case LZTYPE_NUM:
      *t = (*t)->next;
      break;
    case LZTYPE_IDENT:
      *t = (*t)->next;
      break;
    case LZTYPE_LPAREN:
      *t = (*t)->next;
      p_expression(t);
      if ((*t)->type == LZTYPE_RPAREN) {
        *t = (*t)->next;
      } else {
        log_error("Syntax Error: expected ')' but got %s", (*t)->lexeme);
      }
      break;
    default:
        log_error("Syntax Error: expected number, variable reference, or '(' but got %s", (*t)->lexeme);
      break;
  }
}

