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

static tnode_s *parse_header(p_context_s *context);
static StrMap *parse_property_list(p_context_s *context);
static void parse_property_list_(p_context_s *context, StrMap *map);
static void parse_property(p_context_s *context, StrMap *map);
static tnode_s *parse_body(p_context_s *context);
static void parse_statement_list(p_context_s *context, tnode_list_s *stmtlist);
static tnode_s *parse_statement(p_context_s *context);
static tnode_s *parse_declaration(p_context_s *context);
static void parse_opt_assign(p_context_s *context, tnode_s **pident);
static tnode_s *parse_type(p_context_s *context);
static tnode_s *parse_basic_type(p_context_s *context);
static void parse_opt_array(p_context_s *context, tnode_s **ptype);
static tnode_s *parse_assignstmt(p_context_s *context);
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
static tnode_s *tnode_init_basictype(tok_s *val);
static tnode_s *tnode_init_val(tok_s *val);
static tnode_s *tnode_init_bin(tnode_s *l, tok_s *val, p_nodetype_e type, tnode_s *r);
static tnode_s *tnode_init_child(p_nodetype_e type, tok_s *val, tnode_s *child);
static tnode_s *tnode_init_children(p_nodetype_e type, tok_s *val, tnode_list_s children);
static tnode_s *tnode_init_dict(p_nodetype_e type, tok_s *val, StrMap *dict);
static tnode_s *tnode_init_func(tnode_s *funcref, tnode_list_s callargs, tok_s *t);
static int tnode_listadd(tnode_list_s *list, tnode_s *child);
static void report_syntax_error(const char *message, p_context_s *context);
static void report_semantics_error(const char *message, p_context_s *context);
static void print_parse_node(tnode_s *root, CharBuf *buf);
static void print_bin_op(tnode_s *root, CharBuf *buf);
static void print_child(tnode_s *root, CharBuf *buf);
static void print_dict(tnode_s *root, CharBuf *buf);
static void print_list(tnode_s *root, CharBuf *buf);
static void print_leaf(tnode_s *root, CharBuf *buf);
static void print_func(tnode_s *root, CharBuf *buf);
static void print_add_depth(CharBuf *buf);
static void print_pop_depth(CharBuf *buf);

/* 
 * function:	parse	
 * -------------------------------------------------- 
 */
p_context_s parse(toklist_s *list) {
	tnode_s *header = NULL,
					*body = NULL;
	p_context_s context;
	context.currtok = list->head;
	context.parse_errors = 0;
	context.root = NULL;
	bob_str_map_init(&context.symtable);
	header = parse_header(&context);
	body = parse_body(&context);
	if (context.currtok->type != TOK_EOF) {
		report_syntax_error("Expected end of source file", &context);
	}
	context.root = tnode_init_bin(header, list->head, PTYPE_ROOT, body);
	return context;
}

/* 
 * function:	parse_header
 * -------------------------------------------------- 
 */
tnode_s *parse_header(p_context_s *context) {
	return parse_object(context);
}

/* 
 * function:	parse_property_list	
 * -------------------------------------------------- 
 */
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

/* 
 * function:	parse_property_list_	
 * -------------------------------------------------- 
 */
void parse_property_list_(p_context_s *context, StrMap *map) {
	if (context->currtok->type == TOK_COMMA) {
		parse_next_tok(context);
		parse_property(context, map);
		parse_property_list_(context, map);
	}
}

/* 
 * function:	parse_property	
 * -------------------------------------------------- 
 */
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

/* 
 * function:	parse_body	
 * -------------------------------------------------- 
 */
tnode_s *parse_body(p_context_s *context) {
	tnode_s *body = NULL;
	if(context->currtok->type == TOK_LBRACE) {
		tok_s *stmttok = context->currtok;
		tnode_list_s stmtlist = {0, NULL};
		parse_next_tok(context);
		parse_statement_list(context, &stmtlist);	
		if(context->currtok->type != TOK_RBRACE) {
			report_syntax_error("Expected '}'", context);
		}
		body = tnode_init_children(PTYPE_STATEMENTLIST, stmttok, stmtlist);
	}
	else {
		report_syntax_error("Expected '{'", context);
	}
	parse_next_tok(context);
	return body;
}

/* 
 * function:	parse_statement_list	
 * -------------------------------------------------- 
 */
void parse_statement_list(p_context_s *context, tnode_list_s *stmtlist) {
	tnode_s *stmt;
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
		case TOK_IDENTIFIER:
			stmt = parse_statement(context);	
			if(context->currtok->type == TOK_SEMICOLON) {
				parse_next_tok(context);
			}
			tnode_listadd(stmtlist, stmt);
			parse_statement_list(context, stmtlist);
			break;
		default:
			break;
	}
}

/* 
 * function:	parse_statement	
 * -------------------------------------------------- 
 */
tnode_s *parse_statement(p_context_s *context) {
	tnode_s *statement = NULL;
	if(context->currtok->type == TOK_IDENTIFIER) {
		statement = parse_assignstmt(context);	
	}
	else {
		statement = parse_declaration(context);
	}
	return statement;
}

/* 
 * function:	parse_declaration	
 * -------------------------------------------------- 
 */
tnode_s *parse_declaration(p_context_s *context) {
	tnode_s *result = NULL;
	tok_s *typetok = context->currtok;
	parse_type(context);
	if(context->currtok->type == TOK_IDENTIFIER) {
		tok_s *identtok = context->currtok;
		char *ident = identtok->lexeme;
		tnode_s	*dectarget = tnode_init_val(identtok);
		parse_next_tok(context);
		parse_opt_assign(context, &dectarget);
		result = tnode_init_child(PTYPE_DEC, typetok, dectarget);	
	}
	else {
		report_syntax_error("Expected identifier", context);
		parse_next_tok(context);
	}
	return result;
}

/* 
 * function:	parse_opt_assign	
 * -------------------------------------------------- 
 */
void parse_opt_assign(p_context_s *context, tnode_s **pident) {
	if(context->currtok->type == TOK_ASSIGN) {
		tnode_s *expression;
		tok_s *op = context->currtok;
		parse_next_tok(context);
		expression = parse_expression(context);
		*pident = tnode_init_bin(*pident, op, PTYPE_ASSIGN, expression);
	}
}

/* 
 * function:	parse_type	
 * -------------------------------------------------- 
 */
tnode_s *parse_type(p_context_s *context) {
	tnode_s *type = parse_basic_type(context);
	parse_opt_array(context, &type);
	return type;
}

/* 
 * function:	parse_basic_type	
 * -------------------------------------------------- 
 */
tnode_s *parse_basic_type(p_context_s *context) {
	tnode_s *type = NULL;
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
			type = tnode_init_basictype(context->currtok);
			parse_next_tok(context);
			break;
		default:
				report_syntax_error(
					"Expected 'Shader', 'Texture', 'Program', 'Model', 'Instance', 'Num', 'Mesh', 'Dict', '*', or 'String'", context);
			parse_next_tok(context);
			break;
	}
	return type;
}

/* 
 * function:	parse_opt_array	
 * -------------------------------------------------- 
 */
void parse_opt_array(p_context_s *context, tnode_s **ptype) {
	if(context->currtok->type == TOK_LBRACK) {
		tok_s *op = context->currtok;
		parse_next_tok(context);
		switch(context->currtok->type) {
			case TOK_ADDOP:
			case TOK_IDENTIFIER:
			case TOK_NUMBER:
			case TOK_LPAREN:
			case TOK_LBRACE:
			case TOK_LBRACK: {
					tnode_s *expression = parse_expression(context);
					*ptype = tnode_init_bin(*ptype, op, PTYPE_ARRAY_DEC, expression);
				}
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
		parse_opt_array(context, ptype);
	}
}

/* 
 * function:	parse_assignstmt	
 * -------------------------------------------------- 
 */
tnode_s *parse_assignstmt(p_context_s *context) {
	tnode_s *result = NULL, *dest;
	tok_s *val = context->currtok;
	dest = tnode_init_val(val);
	parse_next_tok(context);	
	parse_idsuffix(context, &dest);
	if(context->currtok->type == TOK_ASSIGN) {
		tok_s *assign = context->currtok;
		parse_next_tok(context);
		tnode_s *expression = parse_expression(context);	
		result = tnode_init_bin(dest, assign, PTYPE_ASSIGN, expression);
	}
	else {
		report_syntax_error("Expected ':='", context);
	}
	return result;
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
	tnode_s *factor = NULL;
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
			factor = tnode_init_val(context->currtok);
			parse_next_tok(context);
			parse_idsuffix(context, &factor);
			break;
		case TOK_NUMBER:
			factor = tnode_init_val(context->currtok);
			parse_next_tok(context);
			break;
		case TOK_STRING:
			factor = tnode_init_val(context->currtok);
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
	return factor;
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

tnode_s *tnode_init_basictype(tok_s *val) {
	tnode_s *t = malloc(sizeof *t);
	if(!t) {
		perror("Memory Error in malloc() while allocating new basic type tnode");
		return NULL;
	}
	t->val = val;
	t->type = PTYPE_BASIC_DEC;
	return t;
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
	list->list[list->size - 1] = child;
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

void print_parse_tree(p_context_s *context) {
	CharBuf buf;

	char_buf_init(&buf);
	print_parse_node(context->root, &buf);
	char_buf_free(&buf);
}

void print_parse_node(tnode_s *root, CharBuf *buf) {
	switch(root->type) {
		case PTYPE_ROOT:
		case PTYPE_ADDITION:
		case PTYPE_SUBTRACTION:
		case PTYPE_MULTIPLICATION:
		case PTYPE_DIVISION:
		case PTYPE_ASSIGN:
			print_bin_op(root, buf);
			break;
		case PTYPE_NEGATION:
		case PTYPE_POS:
			break;
		case PTYPE_VAL:
			print_leaf(root, buf);
			break;
		case PTYPE_STATEMENTLIST:
		case PTYPE_ARRAY:
			print_list(root, buf);
			break;
		case PTYPE_OBJECT:
			print_dict(root, buf);
			break;
		case PTYPE_DEC:
			print_child(root, buf);
			break;
		case PTYPE_CALL:
			print_func(root, buf);
			break;
		case PTYPE_ACCESS_DICT:
		case PTYPE_ACCESS_ARRAY:
		case PTYPE_BASIC_DEC:
		case PTYPE_ARRAY_DEC:
			break;
		default:
			break;
	}
}

void print_bin_op(tnode_s *root, CharBuf *buf) {
	char *lex;
	if(root->type == PTYPE_ROOT) {
		lex = "root";
	} 
	else {
		lex = root->val->lexeme;
	}
	printf("%s--[%s]\n", buf->buffer, lex);
	print_add_depth(buf);
	printf("%s\n", buf->buffer);
	print_parse_node(root->c.b.left, buf);
	printf("%s\n", buf->buffer);
	print_parse_node(root->c.b.right, buf);
	print_pop_depth(buf);
}

void print_child(tnode_s *root, CharBuf *buf) {
	printf("%s--[%s]\n", buf->buffer, root->val->lexeme);
	print_add_depth(buf);
	printf("%s\n", buf->buffer);
	print_parse_node(root->c.child, buf);
	print_pop_depth(buf);
}

void print_dict(tnode_s *root, CharBuf *buf) {
	int i;
	StrMap *map = root->c.dict;
	StrMapEntry *entry;
	printf("%s--[object]\n", buf->buffer);
	print_add_depth(buf);
	for(i = 0; i < MAP_TABLE_SIZE; i++) {
		entry = map->table[i];
		while(entry) {
			printf("%s\n", buf->buffer);
			printf("%s--[%s]\n", buf->buffer, entry->key);
			print_add_depth(buf);
			printf("%s\n", buf->buffer);
			print_parse_node(entry->val, buf);
			print_pop_depth(buf);
			entry = entry->next;
		}
	}
	print_pop_depth(buf);
}

void print_list(tnode_s *root, CharBuf *buf) {
	int i;
	tnode_list_s *children = &root->c.children;
	printf("%s--[nodelist]\n", buf->buffer);
	print_add_depth(buf);
	for(i = 0; i < children->size; i++) {
		printf("%s\n", buf->buffer);
		print_parse_node(children->list[i], buf);
	}
	print_pop_depth(buf);
}

void print_leaf(tnode_s *root, CharBuf *buf) {
	printf("%s--[%s]\n", buf->buffer, root->val->lexeme);
}

void print_func(tnode_s *root, CharBuf *buf) {
	int i;
	tnode_list_s *args = &root->c.f.callargs;
	printf("%s--[call]\n", buf->buffer);
	print_add_depth(buf);
	printf("%s\n", buf->buffer);
	print_parse_node(root->c.f.funcref, buf);
	print_add_depth(buf);
	if(args->size == 0) {
		printf("%s--[void]\n", buf->buffer);
	}
	else {
		for(i = 0; i < args->size; i++) {
			printf("%s\n", buf->buffer);
			print_parse_node(args->list[i], buf);
		}
	}
	print_pop_depth(buf);
	print_pop_depth(buf);
}

void print_add_depth(CharBuf *buf) {
  char_add_c(buf, ' ');
  char_add_c(buf, ' ');
  char_add_c(buf, '|');
}

void print_pop_depth(CharBuf *buf) {
	char_popback_c(buf);
	char_popback_c(buf);
	char_popback_c(buf);
}

