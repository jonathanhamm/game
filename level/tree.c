#include "tree.h"
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

typedef struct symcontext_s symcontext_s;

struct symcontext_s {
	int result;
	int count;
	p_context_s *context;
};

static void tree_resolve_symbols(p_context_s *oldcontext);
static bool tree_sym_pass(tnode_s *root, symcontext_s *context);
static bool tree_sym_statement_list(tnode_s *statement_list, symcontext_s *context);
static bool tree_sym_statement(tnode_s *statement, symcontext_s *context);
static bool tree_sym_assign(tnode_s *assign, symcontext_s *context);
static bool tree_sym_check_expression(tnode_s *rval, symcontext_s *context);
static bool tree_nodelist_check(tnode_list_s *children, symcontext_s *context);
static bool tree_dict_check(StrMap *dict, symcontext_s *context);
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
		context.result = tree_sym_pass(oldcontext->root, &context);
		oldcount = context.count;	
	} while(!context.result);
	puts("walking tree complete");
}

bool tree_sym_pass(tnode_s *root, symcontext_s *context) {
	return tree_sym_statement_list(root->c.b.right, context);
}

bool tree_sym_statement_list(tnode_s *statement_list, symcontext_s *context) {
	int i;
	bool result = true;
	tnode_list_s *l = &statement_list->c.children;

	for(i = 0; i < l->size; i++) {
		result = tree_sym_statement(l->list[i], context) && result;
	}
	return result;
}

bool tree_sym_statement(tnode_s *statement, symcontext_s *context) {
	bool result;
	if(statement->type == PTYPE_DEC) {
		tnode_s *right = statement->c.b.right;
		if(right->type == PTYPE_ASSIGN) {
			result = tree_sym_assign(right, context);
		}
		else {
			return true;
		}
	}
	else if(statement->type == PTYPE_ASSIGN) {
		result = tree_sym_assign(statement, context);
	}
	return result;
}

bool tree_sym_assign(tnode_s *assign, symcontext_s *context) {
	bool result;
	tnode_s *lval = assign->c.b.left;
	tnode_s *rval = assign->c.b.right;
	symtable_node_s *symnode;

	result = tree_sym_check_expression(rval, context);
	symnode = bob_str_map_get(&context->context->symtable, lval->val->lexeme);
	if(symnode) {
		if (!symnode->evalflag && result) {
			context->count++;
			printf("resolved symbol: %s\n----------\n", lval->val->lexeme);
			print_parse_subtree(rval);
			puts("----------");
		}
		symnode->evalflag = result;
		symnode->node = rval;
	}
	else {
		fprintf(stderr, "Error: identifier not found in lookup table: %s\n", lval->val->lexeme);
		result = false;
	}
	return result;
}

bool tree_sym_check_expression(tnode_s *val, symcontext_s *context) {
	tnode_s *val1, *val2;
	switch(val->type) {
		case PTYPE_ADDITION:
		case PTYPE_SUBTRACTION:
		case PTYPE_MULTIPLICATION:
		case PTYPE_DIVISION:
		case PTYPE_ACCESS_ARRAY:
		case PTYPE_ACCESS_DICT:
			val1 = val->c.b.left;
			val2 = val->c.b.right;
			return 
				tree_sym_check_expression(val1, context)
				&& tree_sym_check_expression(val2, context);
			break;
		case PTYPE_NEGATION:
		case PTYPE_POS:
			return tree_sym_check_expression(val->c.child, context);
		case PTYPE_VAL:
			return tree_leaf_check(val, context);
		case PTYPE_ARRAY:
			return tree_nodelist_check(&val->c.children, context);
		case PTYPE_OBJECT:
			return tree_dict_check(val->c.dict, context);
		case PTYPE_CALL:
			return tree_sym_check_expression(val->c.f.funcref, context)
						&& tree_nodelist_check(&val->c.f.callargs, context);
		default:
			fprintf(stderr, "Illegal state: %s\n", __func__);
			assert(false);
			return false;
	}
}

bool tree_nodelist_check(tnode_list_s *children, symcontext_s *context) {
	int i;
	bool result = true;

	for(i = 0; i < children->size; i++) {
		result = tree_sym_check_expression(children->list[i], context) && result;
	}
	return result;
}

bool tree_dict_check(StrMap *dict, symcontext_s *context) {
	int i;
	bool result = true;

	for(i = 0; i < MAP_TABLE_SIZE; i++) {
		StrMapEntry *entry = dict->table[i];
		while(entry) {
			result = tree_sym_check_expression(entry->val, context) && result;
			entry = entry->next;
		}
	}
	return result;
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
			fprintf(stderr, "Illegal state: %s\n", __func__);
			assert(false);
			return false;
	}
}

