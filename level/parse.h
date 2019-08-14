#ifndef __parse_h__
#define __parse_h__

#include "lex.h"
#include "../common/data-structures.h"

typedef enum p_nodetype_e p_nodetype_e;
typedef struct p_context_s p_context_s;
typedef struct tnode_list_s tnode_list_s;
typedef struct tnode_s tnode_s;

enum p_nodetype_e {
	PTYPE_ADDITION,
	PTYPE_SUBTRACTION,
	PTYPE_MULTIPLICATION,
	PTYPE_DIVISION,
	PTYPE_NEGATION,
	PTYPE_POS,
	PTYPE_VAL,
	PTYPE_ARRAY,
	PTYPE_OBJECT,
	PTYPE_CALL,
	PTYPE_ACCESS_DICT,
	PTYPE_ACCESS_ARRAY,
	PTYPE_BASIC_DEC,
	PTYPE_ARRAY_DEC,
	PTYPE_ASSIGN,
	PTYPE_DEC,
	PTYPE_STATEMENTLIST,
	PTYPE_ROOT,
	PTYPE_ANY
};

struct p_context_s {
	int parse_errors;
	StrMap symtable;
	tnode_s *root;
	tok_s *currtok;
};

struct tnode_list_s {
	int size;
	tnode_s **list;
};

struct tnode_s {
	p_nodetype_e type;
	tok_s *val;
	union {
		struct {
			tnode_s *left, *right;
		} b;
		struct {
			tnode_s *funcref;
			tnode_list_s callargs;
		} f;
		tnode_list_s children;
		tnode_s *child;
		StrMap *dict;
	} c ;
};

extern p_context_s parse(toklist_s *list);
extern void print_parse_tree(p_context_s *context);

#endif

