#include "parse.h"
#include "error.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>


/*
typedef enum p_type_e p_type_e;


enum p_type_e {
	PTYPE_SHADER,
	PTYPE_TEXTURE,
	PTYPE_MESH,
	PTYPE_MODEL,
	PTYPE_INSTANCE,
	PTYPE_NUM,
	PTYPE_STRING,
	PTYPE_DICT,
	PTYPE_ARRAY,
	PTYPE_ANY
};
*/

/*
	Shader <- { Shader, Dict }
	Texture <- { Texture, Dict }
	Mesh <- { Mesh, Dict }
	Model <- { Model, Dict }
	Instance <- { Instance, Dict }
	Num <- { Num }
	String <- { String, Num }
	Dict <- { Dict }
	Array of type <- { Array of type }
	Any <- { any type above }

	* Valid Binary Opertions:

		- Num + Num -> Num
		- Num - Num -> Num
		- Num * Num -> Num
		- Num / Num -> Num
		- - Num -> Num
		- String + String -> String
		- Num + String -> String [commutative]
*/

static void parse_header(p_context_s *context);
static StrMap *parse_property_list(p_context_s *context);
static void parse_property_list_(p_context_s *context, StrMap *map);
static void parse_property(p_context_s *context, StrMap *map);
static void parse_body(p_context_s *context);
static void parse_statement_list(p_context_s *context);
static void parse_statement(p_context_s *context);
static void parse_declaration(p_context_s *context);
static tnode_s *parse_opt_assign(p_context_s *context);
static void parse_type(p_context_s *context);
static void parse_basic_type(p_context_s *context);
static void parse_opt_array(p_context_s *context);
static tnode_s *parse_expression(p_context_s *context);
static void parse_expression_(p_context_s *context, tnode_s **root);
static tnode_s *parse_term(p_context_s *context);
static void parse_term_(p_context_s *context, tnode_s **root);
static tnode_s *parse_factor(p_context_s *context);
static void parse_idsuffix(p_context_s *context, tnode_s **pfactor);
static tnode_list_s parse_expression_list(p_context_s *context);
static void parse_expression_list_(p_context_s *context, tnode_list_s *list);
static tnode_s *parse_object(p_context_s *context);
static tnode_s *parse_array(p_context_s *context);
static void parse_next_tok(p_context_s *context);
static tnode_s *tnode_init_val(tok_s *val);
static tnode_s *tnode_init_bin(tnode_s *l, tok_s *val, p_nodetype_e type, tnode_s *r);
static tnode_s *tnode_init_child(p_nodetype_e type, tok_s *val, tnode_s *child);
static tnode_s *tnode_init_children(p_nodetype_e type, tok_s *val, tnode_list_s children);
static tnode_s *tnode_init_dict(p_nodetype_e type, tok_s *val, StrMap *dict);
static tnode_s *tnode_init_func(tnode_s *funcref, tnode_list_s callargs, tok_s *t);
static int tnode_listadd(tnode_list_s *list, tnode_s *child);
static void report_syntax_error(const char *message, p_context_s *context);
static void report_semantics_error(const char *message, p_context_s *context);

p_context_s parse(toklist_s *list) {
	p_context_s context;

	context.currtok = list->head;
	context.parse_errors = 0;
	bob_str_map_init(&context.symtable);
	parse_header(&context);
	parse_body(&context);
	if (context.currtok->type != TOK_EOF) {
		report_syntax_error("Expected end of source file", &context);
	}
}

void parse_header(p_context_s *context) {
	parse_object(context);
}

StrMap *parse_property_list(p_context_s *context) {
	StrMap *result = calloc(1, sizeof(*result));
	if(!result) {
		perror("Memory allocation error with calloc while creating new property list");
		return NULL;
	}
	if(context->currtok->type == TOK_STRING) {
		parse_property(context, result);
		parse_property_list_(context, result);
	}
	return result;
}

void parse_property_list_(p_context_s *context, StrMap *map) {
	if (context->currtok->type == TOK_COMMA) {
		parse_next_tok(context);
		parse_property(context, map);
		parse_property_list_(context, map);
	}
}

void parse_property(p_context_s *context, StrMap *map) {
	if (context->currtok->type == TOK_STRING) {
		char *key = context->currtok->lexeme;
		parse_next_tok(context);
		if (context->currtok->type == TOK_COLON) {
			parse_next_tok(context);
			tnode_s *expression = parse_expression(context);
			bob_str_map_insert(map, key, expression);
		}
		else {
			report_syntax_error("Expected ':'", context);
			parse_next_tok(context);
		}
	}
	else {
		report_syntax_error("Expected string", context);
		parse_next_tok(context);
	}
}

void parse_body(p_context_s *context) {
	if (context->currtok->type == TOK_LBRACE) {
		parse_next_tok(context);
		parse_statement_list(context);	
		if (context->currtok->type != TOK_RBRACE) {
			report_syntax_error("Expected '}'", context);
		}
	}
	else {
		report_syntax_error("Expected '{'", context);
	}
	parse_next_tok(context);
}

void parse_statement_list(p_context_s *context) {
	switch(context->currtok->type) {
		case TOK_SHADER_DEC:
		case TOK_TEXTURE_DEC:
		case TOK_PROGRAM_DEC:
		case TOK_MESH_DEC:
		case TOK_MODEL_DEC:
		case TOK_INSTANCE_DEC:
		case TOK_NUM_DEC:
		case TOK_DICT_DEC:
		case TOK_STRING_DEC:
		case TOK_GENERIC_DEC:
			parse_statement(context);	
			if(context->currtok->type == TOK_SEMICOLON) {
				parse_next_tok(context);
			}
			parse_statement_list(context);
			break;
		default:
			break;
	}
}

void parse_statement(p_context_s *context) {
	parse_declaration(context);
}

void parse_declaration(p_context_s *context) {
	parse_type(context);
	if(context->currtok->type == TOK_IDENTIFIER) {
		char *ident = context->currtok->lexeme;
		parse_next_tok(context);
		tnode_s *tnode =  parse_opt_assign(context);
		bob_str_map_insert(&context->symtable, ident, tnode); 
	}
	else {
		report_syntax_error("Expected identifier", context);
		parse_next_tok(context);
	}
}

tnode_s *parse_opt_assign(p_context_s *context) {
	if(context->currtok->type == TOK_ASSIGN) {
		parse_next_tok(context);
		return parse_expression(context);
	}
	return NULL;
}

void parse_type(p_context_s *context) {
	parse_basic_type(context);
	parse_opt_array(context);
}

void parse_basic_type(p_context_s *context) {
	switch(context->currtok->type) {
		case TOK_SHADER_DEC:
		case TOK_TEXTURE_DEC:
		case TOK_PROGRAM_DEC:
		case TOK_MESH_DEC:
		case TOK_MODEL_DEC:
		case TOK_INSTANCE_DEC:
		case TOK_NUM_DEC:
		case TOK_DICT_DEC:
		case TOK_STRING_DEC:
		case TOK_GENERIC_DEC:
			parse_next_tok(context);
			break;
		default:
				report_syntax_error(
					"Expected 'Shader', 'Texture', 'Program', 'Model', 'Instance', 'Num', 'Mesh', 'Dict', '*', or 'String'", context);
			parse_next_tok(context);
			break;
	}
}

void parse_opt_array(p_context_s *context) {
	if(context->currtok->type == TOK_LBRACK) {
		parse_next_tok(context);
		switch(context->currtok->type) {
			case TOK_ADDOP:
				if(!strcmp(context->currtok->lexeme, "+") || !strcmp(context->currtok->lexeme, "-")) {
					parse_expression(context);
				}
				break;
			case TOK_IDENTIFIER:
			case TOK_NUMBER:
			case TOK_LPAREN:
			case TOK_LBRACE:
			case TOK_LBRACK:
				parse_expression(context);
				break;
			default:
				break;
		}
		if(context->currtok->type == TOK_RBRACK) {
			parse_next_tok(context);
		}
		else {
			report_syntax_error("Expected ']'", context);
			parse_next_tok(context);
		}
		parse_opt_array(context);
	}
}

/* 
 * function:	parse_expression	
 * -------------------------------------------------- 
 */
tnode_s *parse_expression(p_context_s *context) {
	tnode_s *root = parse_term(context);
	parse_expression_(context, &root);
	return root;
}

/* 
 * function:	parse_expression_
 * -------------------------------------------------- 
 */
void parse_expression_(p_context_s *context, tnode_s **root) {
	if (context->currtok->type == TOK_ADDOP) {
		tok_s *addop = context->currtok;
		parse_next_tok(context);
		tnode_s *term = parse_term(context);
		if(!strcmp(addop->lexeme, "+"))
			*root = tnode_init_bin(*root, addop, PTYPE_ADDITION, term); 
		else
			*root = tnode_init_bin(*root, addop, PTYPE_SUBTRACTION, term);
		parse_next_tok(context);
		parse_expression_(context, root);
	}
}

/* 
 * function:	parse_term
 * -------------------------------------------------- 
 */
tnode_s *parse_term(p_context_s *context) {
	tnode_s *root = parse_factor(context);
	parse_term_(context, &root);
	return root;
}

/* 
 * function:	parse_term_	
 * -------------------------------------------------- 
 */
void parse_term_(p_context_s *context, tnode_s **root) {
	if (context->currtok->type == TOK_MULOP) {
		tok_s *mulop = context->currtok;
		parse_next_tok(context);
		tnode_s *factor = parse_factor(context);
		if(!strcmp(mulop->lexeme, "*"))
			*root = tnode_init_bin(*root, mulop, PTYPE_MULTIPLICATION, factor);
		else
			*root = tnode_init_bin(*root, mulop, PTYPE_DIVISION, factor);
		parse_term_(context, root);
	}
}

/* 
 * function:	parse_factor	
 * -------------------------------------------------- 
 */
tnode_s *parse_factor(p_context_s *context) {
	tnode_s *factor;
	switch(context->currtok->type) {
		case TOK_ADDOP: {
				tok_s *addop = context->currtok;
				parse_next_tok(context);
				tnode_s *expression = parse_expression(context);
				if(!strcmp(addop->lexeme, "-"))
					factor = tnode_init_child(PTYPE_NEGATION, addop, expression);
				else
					factor = tnode_init_child(PTYPE_POS, addop, expression);
				parse_idsuffix(context, &factor);
			}
			break;
		case TOK_IDENTIFIER:
			factor = tnode_init_bin(NULL, context->currtok, PTYPE_VAL, NULL);
			parse_next_tok(context);
			parse_idsuffix(context, &factor);
			break;
		case TOK_NUMBER:
			factor = tnode_init_bin(NULL, context->currtok, PTYPE_VAL, NULL);
			parse_next_tok(context);
			break;
		case TOK_STRING:
			factor = tnode_init_bin(NULL, context->currtok, PTYPE_VAL, NULL);
			parse_next_tok(context);
			parse_idsuffix(context, &factor);
			break;
		case TOK_LPAREN: {
				parse_next_tok(context);
				factor = parse_expression(context);
				if(context->currtok->type != TOK_RPAREN) {
					report_syntax_error("Expected ')'", context);
				}
				parse_next_tok(context);
				parse_idsuffix(context, &factor);
			}
			break;
		case TOK_LBRACE:
			factor = parse_object(context);
			parse_idsuffix(context, &factor);
			break;
		case TOK_LBRACK:
			factor = parse_array(context);
			parse_idsuffix(context, &factor);
			break;
		default:
			report_syntax_error("Expected identifier, number, string, '+', '-', '(', '{', or '['", context);
			parse_next_tok(context);
			break;
	}
	return NULL;
}

/* 
 * function:	parse_idsuffix	
 * -------------------------------------------------- 
 */
void parse_idsuffix(p_context_s *context, tnode_s **pfactor) {
	tok_s *op;
	switch(context->currtok->type) {
		case TOK_DOT:
			op = context->currtok;
			parse_next_tok(context);
			if(context->currtok->type == TOK_IDENTIFIER) {
				tok_s *tt = context->currtok;
				tnode_s *val = tnode_init_val(tt);
				*pfactor = tnode_init_bin(*pfactor, op, PTYPE_ACCESS_DICT, val);
				parse_next_tok(context);
				parse_idsuffix(context, pfactor);
			}
			else {
				report_syntax_error("Expected identifier", context);
				parse_next_tok(context);
			}
			break;
		case TOK_LPAREN: {
				tnode_list_s expression_list;
				op = context->currtok;
				parse_next_tok(context);
				expression_list = parse_expression_list(context);
				if(context->currtok->type == TOK_RPAREN) {
					*pfactor = tnode_init_func(*pfactor, expression_list, op);
					parse_next_tok(context);
					parse_idsuffix(context, pfactor);
				}
				else {
					report_syntax_error("Expected ')'", context);
					parse_next_tok(context);
				}
			}
			break;
		case TOK_LBRACK: {
				tnode_s *expression;
				op = context->currtok;
				parse_next_tok(context);
				expression = parse_expression(context);
				if(context->currtok->type == TOK_RBRACK) {
					parse_next_tok(context);
					*pfactor = tnode_init_bin(*pfactor, op, PTYPE_ACCESS_ARRAY, expression);
					parse_idsuffix(context, pfactor);
				}
				else {
					report_syntax_error("Expected ']'", context);
					parse_next_tok(context);
				}
			}
			break;
		default:
			break;
	}
}

/* 
 * function:	parse_expression_list
 * -------------------------------------------------- 
 */
tnode_list_s parse_expression_list(p_context_s *context) {
	tnode_list_s list = {0, NULL};
	tnode_s *expression;
	switch(context->currtok->type) {
		case TOK_ADDOP:
			if(!strcmp(context->currtok->lexeme, "+") || !strcmp(context->currtok->lexeme, "-")) {
				expression = parse_expression(context);
				tnode_listadd(&list, expression);
				parse_expression_list_(context, &list);
			}
			break;
		case TOK_IDENTIFIER:
		case TOK_NUMBER:
		case TOK_STRING:
		case TOK_LPAREN:
		case TOK_LBRACE:
		case TOK_LBRACK:
			expression = parse_expression(context);
			tnode_listadd(&list, expression);
			parse_expression_list_(context, &list);
			break;
		default:
			break;
	}
	return list;
}

/* 
 * function:	parse_expression_list_	
 * -------------------------------------------------- 
 */
void parse_expression_list_(p_context_s *context, tnode_list_s *list) {
	if(context->currtok->type == TOK_COMMA) {
		tnode_s *expression;
		parse_next_tok(context);
		expression = parse_expression(context);
		tnode_listadd(list, expression);
		parse_expression_list_(context, list);
	}
}

/* 
 * function:	parse_object
 * -------------------------------------------------- 
 */
tnode_s *parse_object(p_context_s *context) {
	tnode_s *result = NULL;
	if(context->currtok->type == TOK_LBRACE) {
		tok_s *object = context->currtok;
		parse_next_tok(context);
		StrMap *dict = parse_property_list(context);
		result = tnode_init_dict(PTYPE_OBJECT, object, dict);
		if(context->currtok->type == TOK_RBRACE) {
			parse_next_tok(context);
		}
		else {
			report_syntax_error("Expected '}'", context);
			parse_next_tok(context);
		}
	}
	else {
		report_syntax_error("Expected '{'", context);
		parse_next_tok(context);
	}
	return result;
}


/* 
 * function:	parse_aray 
 * -------------------------------------------------- 
 */
tnode_s *parse_array(p_context_s *context) {
	tnode_s *result = NULL;
	if(context->currtok->type == TOK_LBRACK) {
		tok_s *array = context->currtok;
		parse_next_tok(context);
		tnode_list_s list = parse_expression_list(context);
		result = tnode_init_children(PTYPE_ARRAY, array, list);
		if(context->currtok->type == TOK_RBRACK) {
			parse_next_tok(context);
		}
		else {
			report_syntax_error("Expected ']'", context);
			parse_next_tok(context);
		}
	}
	else {
		report_syntax_error("Expected '['", context);
		parse_next_tok(context);
	}
	return result;
}

void parse_next_tok(p_context_s *context) {
	if(context->currtok->type != TOK_EOF)
		context->currtok = context->currtok->next;
}

tnode_s *tnode_init_val(tok_s *val) {
	tnode_s *t = malloc(sizeof *t);
	if(!t) {
		perror("Memory Error in malloc() while allocating new val tnode");
		return NULL;
	}
	t->val = val;
	t->type = PTYPE_VAL;
	return t;
}

static tnode_s *tnode_init_bin(tnode_s *l, tok_s *val, p_nodetype_e type, tnode_s *r) {
	tnode_s *t = malloc(sizeof *t);
	if(!t) {
		perror("Memory Error in malloc() while allocating new binary tnode");
		return NULL;
	}
	t->c.b.left = l;
	t->val = val;
	t->type = type;
	t->c.b.right = r;
	return t;
}

tnode_s *tnode_init_child(p_nodetype_e type, tok_s *val, tnode_s *child) {
	tnode_s *t = malloc(sizeof *t);
	if(!t) {
		perror("Memory Error in malloc() while allocating new tnode with child");
		return NULL;
	}
	t->c.child = child;
	t->val = val;
	t->type = type;
	return t;
}

tnode_s *tnode_init_children(p_nodetype_e type, tok_s *val, tnode_list_s children) {
	tnode_s *t = malloc(sizeof *t);
	if(!t) {
		perror("Memory Error in malloc() while allocating new tnode with children");
		return NULL;
	}
	t->c.children = children;
	t->val = val;
	t->type = type;
	return t;
}

tnode_s *tnode_init_dict(p_nodetype_e type, tok_s *val, StrMap *dict) {
	tnode_s *t = malloc(sizeof *t);
	if(!t) {
		perror("Memory Error in malloc() while allocating a new tnode with dictionary");
		return NULL;
	}
	t->c.dict = dict;
	t->val = val;
	t->type = type;
	return t;
}

tnode_s *tnode_init_func(tnode_s *funcref, tnode_list_s callargs, tok_s *t) {
	tnode_s *f = malloc(sizeof *f);
	if(!f) {
		perror("Memory Error in malloc() while allocating a new tnode for function call");
		return NULL;
	}
	f->c.f.funcref = funcref;
	f->c.f.callargs = callargs;
	f->val = t;
	f->type = PTYPE_CALL;
	return f;
}

int tnode_listadd(tnode_list_s *list, tnode_s *child) {
	tnode_s **nlist;

	list->size++;
	nlist = realloc(list->list, sizeof(*list->list) * list->size);
	if(!nlist) {
		perror("Memory Error in realloc() while adding tnode to tnode list");
		list->size--;
		return -1;
	}
	list->list = nlist;
	return 0;
}

void report_syntax_error(const char *message, p_context_s *context) {
	tok_s *t = context->currtok;
	context->parse_errors++;
	fprintf(stderr, "Syntax Error at line %u, token: '%s': %s\n", t->lineno, t->lexeme, message);
}

void report_semantics_error(const char *message, p_context_s *context) {
	tok_s *t = context->currtok;
	context->parse_errors++;
	fprintf(stderr, "Error at line %u, token '%s': %s\n", t->lineno, t->lexeme, message);
}

