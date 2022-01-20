#ifndef __parse_h__
#define __parse_h__

#include "lex.h"
#include "../common/data-structures.h"
#include <stdbool.h>
#include <stdio.h>

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
	PTYPE_MESH,
	PTYPE_MODEL,
	PTYPE_INSTANCE,
  PTYPE_LAZY_INSTANCE,
  PTYPE_RANGE,
	PTYPE_OBJECT,
	PTYPE_ARRAY,
	PTYPE_LEVEL,
	PTYPE_ANY,
	/* Type Nodes */
	PTYPE_SHADER_DEC,
	PTYPE_TEXTURE_DEC,
	PTYPE_PROGRAM_DEC,
	PTYPE_MESH_DEC,
	PTYPE_MODEL_DEC,
	PTYPE_INSTANCE_DEC,
  PTYPE_LAZY_INSTANCE_DEC,
  PTYPE_RANGE_DEC,
	PTYPE_INT_DEC,
	PTYPE_FLOAT_DEC,
	PTYPE_DICT_DEC,
	PTYPE_STRING_DEC,
	PTYPE_GENERIC_DEC,
	PTPYE_INT_DEC,
	PTYPE_ARRAY_DEC,
	PTYPE_LEVEL_DEC
};

struct p_context_s {
	int parse_errors;
  unsigned labelcount;
	StrMap symtable;
	tnode_s *root;
	tok_s *currtok;
  CharBuf meshcode;
  CharBuf shadercode;
  CharBuf programcode;
  CharBuf texturecode;
  CharBuf modelcode;
	CharBuf levelcode;
  CharBuf instancecode;
  CharBuf lazyinstancecode;
  CharBuf rangeCode;
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
	tok_s *tok;
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
extern void gen_code(p_context_s *context, FILE *dest);

#endif

