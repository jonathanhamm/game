#include "lazy_instance_engine.h"

#include "all.h"

#include <ctype.h>
#include <stdlib.h>


#define LZTOK_LEX_LEN 64

typedef enum lztok_type_e lztok_type_e;

typedef struct lztok_s lztok_s;

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

static lztok_s *lz_add_tok(lztok_s **curr, char *lexeme);

double lazy_epxression_compute(char *src, double val) {
	return 0;
}

lztok_s *lex(char *src) {
	char bck;
	char *fptr = src, *bptr = src;

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
				fptr++;
				break;
			case '-':
				fptr++;
				break;
			case '*':
				fptr++;
				break;
			case '/':
				fptr++;
				break;
			default:
				if (isdigit(*fptr)) {
					bptr = fptr;
					while (isdigit(*++fptr));
					bck = *fptr;
					*fptr = '\0';
				} 
				else if (isalpha(*fptr)) {
				}
				else {
				}
				break;
		}
	}

}

void lz_add_tok(lztok_s **curr, char *lexeme) {
	lztok_s *ntok = malloc(sizeof *ntok);
	if (!ntok) {
		log_error("failed to allocate memory for new lz token");
		//TODO: trap
	}
	ntok->

}


