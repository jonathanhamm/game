#ifndef __error_h__
#define __error_h__

#include "lex.h"
#include <stdio.h>

extern void report_lexical_error(const char *message, char c, unsigned lineno);
extern void report_syntax_error(const char *message, tok_s *tok);


#endif

