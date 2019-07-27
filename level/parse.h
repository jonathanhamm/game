#ifndef __parse_h__
#define __parse_h__

#include "lex.h"
#include "../common/data-structures.h"

typedef enum p_nodetype_e p_nodetype_e;
typedef struct p_context_s p_context_s;
typedef struct tnode_s tnode_s;

enum p_nodetype_e {
	PTYPE_ADDITION,
	PTYPE_SUBTRACTION,
	PTYPE_MULTIPLICATION,
	PTYPE_DIVISION,
	PTYPE_NEGATION,
	PTYPE_POS,
	PTYPE_VAL
};

struct p_context_s {
	int parse_errors;
	StrMap symtable;
	tnode_s *root;
	tok_s *currtok;
};

struct tnode_s {
	p_nodetype_e type;
	tok_s *val;
	tnode_s *l, *r;
};

extern p_context_s parse(toklist_s *list);

#endif

