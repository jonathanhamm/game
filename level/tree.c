#include "tree.h"
#include <stdio.h>

typedef struct symcontext_s symcontext_s;

struct symcontext_s {
	int result;
	int count;
	p_context_s *context;
};

static void tree_resolve_symbols(p_context_s *oldcontext);
static void tree_sym_pass(tnode_s *root, symcontext_s *context);
static void tree_sym_statement_list(tnode_s *statement_list, symcontext_s *context);
static void tree_sym_statement(tnode_s *statement, symcontext_s *context);
static void tree_sym_assign(tnode_s *assign, symcontext_s *context);

void tree_walk(p_context_s *context) {
	tree_resolve_symbols(context);
}

void tree_resolve_symbols(p_context_s *oldcontext) {
	int result, oldcount = -1;
	symcontext_s context;

	context.result = 1;
	context.count = 0;
	context.context = oldcontext;
	do {
		if(oldcount == context.count) {
			perror("Error: Could not resolve all symtable symbols");
			break;
		}
		tree_sym_pass(oldcontext->root, &context);
		context.result = 1;
		oldcount = context.count;	
	} while(!context.result);
}

void tree_sym_pass(tnode_s *root, symcontext_s *context) {
	tree_sym_statement_list(root->c.b.right, context);
}

void tree_sym_statement_list(tnode_s *statement_list, symcontext_s *context) {
	int i;
	tnode_list_s *l = &statement_list->c.children;

	for(i = 0; i < l->size; i++) {
		tree_sym_statement(l->list[i], context);
	}
}

void tree_sym_statement(tnode_s *statement, symcontext_s *context) {
	if(statement->type == PTYPE_DEC) {
	}
	else if(statement->type == PTYPE_ASSIGN) {
		//bob_str_map_get(
	}
}


void tree_sym_assign(tnode_s *assign, symcontext_s *context) {
}

