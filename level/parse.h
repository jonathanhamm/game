#ifndef __parse_h__
#define __parse_h__

#include "lex.h"
#include "../common/data-structures.h"
#include <stdbool.h>

typedef enum p_nodetype_e p_nodetype_e;
typedef struct p_context_s p_context_s;
typedef struct tnode_list_s tnode_list_s;
typedef struct tnode_arraytype_val_s tnode_arraytype_val_s;
typedef struct tnode_s tnode_s;
typedef struct symtable_node_s symtable_node_s;

enum p_nodetype_e {
	PTYPE_INT,
	PTYPE_FLOAT,
	PTYPE_STRING,
	PTYPE_SHADER,
	PTYPE_TEXTURE,
	PTYPE_PROGRAM,
	PTYPE_OBJECT,
	PTYPE_ARRAY,
	/* Type Nodes */
	PTYPE_SHADER_DEC,
	PTYPE_TEXTURE_DEC,
	PTYPE_PROGRAM_DEC,
	PTYPE_MESH_DEC,
	PTYPE_MODEL_DEC,
	PTYPE_INSTANCE_DEC,
	PTYPE_INT_DEC,
	PTYPE_FLOAT_DEC,
	PTYPE_DICT_DEC,
	PTYPE_STRING_DEC,
	PTYPE_GENERIC_DEC,
	PTPYE_INT_DEC,
	PTYPE_ARRAY_DEC,
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

struct tnode_arraytype_val_s {
	p_nodetype_e type;
	tnode_list_s arr;
};

struct tnode_s {
	p_nodetype_e type;
	unsigned lineno;
	union {
		char *s;
		int i;
		double f;
		StrMap *obj;
		tnode_list_s list;
		tnode_arraytype_val_s atval;
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

