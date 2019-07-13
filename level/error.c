#include "error.h"


void report_lexical_error(const char *message, char c, unsigned lineno) {
	fprintf(stderr, "Lexical Error at line %u, character %c: %s\n", lineno, c, message);
}

void report_syntax_error(const char *message, tok_s *tok) {
	fprintf(stderr, "Syntax Error at line %u, token: %s: %s\n", tok->lineno, tok->lexeme, message);
}

