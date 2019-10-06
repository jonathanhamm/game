#ifndef __parse_h__
#define __parse_h__

#include "lex.h"
#include "../common/data-structures.h"
#include <stdbool.h>

typedef enum p_nodetype_e p_nodetype_e;
typedef struct p_context_s p_context_s;
typedef struct tnode_list_s tnode_list_s;
typedef struct tnode_s tnode_s;
typedef struct symtable_node_s symtable_node_s;

enum p_nodetype_e {
	PTYPE_INT,
	PTYPE_FLOAT,
	PTYPE_STRING,
	PTYPE_SHADER,
	PTYPE_TEXTURE,
	PTYPE_PROGRAM,
	PTYPE_OBJECT
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
	unsigned lineno;
	union {
		char *s;
		int i;
		double f;
		StrMap map;
	} val;
};

struct symtable_node_s {
	bool evalflag;
	tnode_s *node;
};

extern p_context_s parse(toklist_s *list);
extern void print_parse_tree(p_context_s *context);
extern void print_parse_subtree(tnode_s *root);

#endif

