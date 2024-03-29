#ifndef __lex_h__
#define __lex_h__

#include "../common/data-structures.h"

typedef struct tok_s tok_s;
typedef struct toklist_s toklist_s;

typedef enum {
	TOK_NONE,
	TOK_IDENTIFIER,
	TOK_STRING,
	TOK_INTEGER,
	TOK_FLOAT,
	TOK_LPAREN,
	TOK_RPAREN,
	TOK_LBRACE,
	TOK_RBRACE,
	TOK_LBRACK,
	TOK_RBRACK,
	TOK_COMMA,
	TOK_COLON,
	TOK_SEMICOLON,
	TOK_ASSIGN,
	TOK_DOT,
	TOK_ADDOP,
	TOK_MULOP,
	TOK_SHADER_DEC,
	TOK_TEXTURE_DEC,
	TOK_PROGRAM_DEC,
	TOK_MESH_DEC,
	TOK_MODEL_DEC,
	TOK_INSTANCE_DEC,
	TOK_LEVEL_DEC,
	TOK_INT_DEC,
	TOK_FLOAT_DEC,
	TOK_STRING_DEC,
	TOK_DICT_DEC,
	TOK_GENERIC_DEC,
  TOK_RANGE_DEC,
  TOK_LAZY_INSTANCE_DEC,
	TOK_EOF
} toktype_e;

struct tok_s {
	toktype_e type;
	unsigned lineno;
	tok_s *next;
	char lexeme[];
};

struct toklist_s {
	tok_s *head;
	tok_s *tail;
};

extern toklist_s lex(const char *file_name);

extern void toklist_print(toklist_s *list);

#endif

