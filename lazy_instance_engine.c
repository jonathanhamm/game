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
static lztok_s *lex(char *src);
static char *dupstr(char *str);

double lazy_epxression_compute(char *src, double val) {
  char *nsrc = dupstr(src);
  lztok_s *toklist = lex(nsrc);
	return 0;
}

lztok_s *lex(char *src) {
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
			default:
				if (isdigit(*fptr)) {
					bptr = fptr;
					while (isdigit(*++fptr));
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

  lztok_s *t = toklist.head;
  while (t) {
    log_info("parsed token: %s - %d", t->lexeme, t->type);
    t = t->next;
  }
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


