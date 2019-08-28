#include "tree.h"
#include <stdio.h>
#include <stdbool.h>

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
static bool tree_sym_check_expression(tnode_s *rval, symcontext_s *context);
static bool tree_leaf_check(tnode_s *leaf, symcontext_s *context);

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
		tnode_s *right = statement->c.b.right;
		if(right->type == PTYPE_ASSIGN) {
			tree_sym_assign(right, context);
		}
	}
	else if(statement->type == PTYPE_ASSIGN) {
		//bob_str_map_get(
	}
}

void tree_sym_assign(tnode_s *assign, symcontext_s *context) {
	bool result;
	tnode_s *lval = assign->c.b.left;
	tnode_s *rval = assign->c.b.right;
	symtable_node_s *symnode;

	result = tree_sym_check_expression(rval, context);
	symnode = bob_str_map_get(&context->context->symtable, lval->val->lexeme);
	if(symnode) {
		symnode->evalflag = true;
		symnode->node = rval;
	}
	else {
		fprintf(stderr, "Error: identifier not found in lookup table: %s\n", lval->val->lexeme);
	}
}

bool tree_sym_check_expression(tnode_s *val, symcontext_s *context) {
	tnode_s *val1, *val2;
	switch(val->type) {
		case PTYPE_ADDITION:
		case PTYPE_SUBTRACTION:
		case PTYPE_MULTIPLICATION:
		case PTYPE_DIVISION:
			val1 = val->c.b.left;
			val2 = val->c.b.right;
			return 
				tree_sym_check_expression(val1, context)
				&& tree_sym_check_expression(val2, context);
			break;
		case PTYPE_ACCESS_ARRAY:
			break;
		case PTYPE_ACCESS_DICT:
			break;
		case PTYPE_NEGATION:
		case PTYPE_POS:
			break;
		case PTYPE_VAL:
			tree_leaf_check(val, context);
			break; 
		default:
			break;
	}
	return false;
}

bool tree_leaf_check(tnode_s *leaf, symcontext_s *context) {
	symtable_node_s *symnode;

	switch(leaf->val->type) {
		case TOK_IDENTIFIER:
			symnode = bob_str_map_get(&context->context->symtable, leaf->val->lexeme);
			if(symnode) {
				return symnode->evalflag;	
			}
			else {
				fprintf(stderr, "Error: identifier not found in lookup table: %s\n", leaf->val->lexeme);
				return false;
			}
		case TOK_NUMBER:
		case TOK_STRING:
			return true;
		default:
			break;
	}
}

