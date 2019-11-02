#include "parse.h"
#include <stdio.h>
#include <string.h>

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
static void parse_opt_assign(p_context_s *context, symtable_node_s *symnode);
static tnode_s *parse_type(p_context_s *context);
static tnode_s *parse_basic_type(p_context_s *context);
static void parse_opt_array(p_context_s *context, tnode_list_s *arr);
static tnode_s *parse_assignstmt(p_context_s *context);
static tnode_s *parse_expression(p_context_s *context);
static void parse_expression_(p_context_s *context, tnode_s *root);
static tnode_s *parse_term(p_context_s *context);
static void parse_term_(p_context_s *context, tnode_s *root);
static tnode_s *parse_factor(p_context_s *context);
static void parse_idsuffix(p_context_s *context, tnode_s **pfactor);
static tnode_list_s parse_expression_list(p_context_s *context);
static void parse_expression_list_(p_context_s *context, tnode_list_s *list);
static tnode_s *parse_object(p_context_s *context);
static tnode_s *parse_array(p_context_s *context);
static void parse_next_tok(p_context_s *context);
static bool exec_negate(tnode_s *operand);
static bool exec_sub(tnode_s *accum, tnode_s *operand);
static bool exec_add(tnode_s *accum, tnode_s *operand);
static bool exec_mult(tnode_s *accum, tnode_s *operand);
static bool exec_div(tnode_s *accum, tnode_s *operand);
static tnode_s *exec_access_obj(tnode_s *obj, char *key);
static tnode_s *exec_access_arr(tnode_s *arr, tnode_s *index);
static tnode_s *sym_lookup(p_context_s *context, char *key);
static char *concat_str_integer(char *str, int i);
static char *concat_integer_str(int i, char *str);
static char *concat_str_double(char *str, double d);
static char *concat_double_str(double d, char *str);
static char *concat_str_str(char *str1, char *str2);
static tnode_s *tnode_create_x(void);
static tnode_s *tnode_create_str(unsigned lineno, char *s);
static tnode_s *tnode_create_int(unsigned lineno, int i);
static tnode_s *tnode_create_float(unsigned lineno, double f);
static tnode_s *tnode_create_object(unsigned lineno, StrMap *obj);
static tnode_s *tnode_create_array(unsigned lineno, tnode_list_s list);
static tnode_s *tnode_create_basic_type(unsigned lineno, p_nodetype_e type);
static tnode_s *tnode_create_array_type(unsigned lineno, tnode_arraytype_val_s atval);
static void tnode_list_init(tnode_list_s *list);
static int tnode_list_add(tnode_list_s *list, tnode_s *node);
static void report_syntax_error(const char *message, p_context_s *context);
static void report_semantics_error(const char *message, p_context_s *context);

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
	if(context->currtok->type == TOK_COMMA) {
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
		case TOK_INT_DEC:
		case TOK_FLOAT_DEC:
		case TOK_DICT_DEC:
		case TOK_STRING_DEC:
		case TOK_GENERIC_DEC:
		case TOK_IDENTIFIER:
			stmt = parse_statement(context);	
			if(context->currtok->type == TOK_SEMICOLON) {
				parse_next_tok(context);
			}
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
	tnode_s *result = NULL, *typenode;
	symtable_node_s *symnode = NULL;
	tok_s *typetok = context->currtok;
	typenode = parse_type(context);
	if(context->currtok->type == TOK_IDENTIFIER) {
		tok_s *identtok = context->currtok;
		char *ident = identtok->lexeme;
		if (!bob_str_map_get(&context->symtable, ident)) {
			symnode = malloc(sizeof *symnode);
			if(!symnode) {
				perror("Memory Allocation error in malloc while allocating a symtable node.");
				return NULL;
			}
			symnode->evalflag = false;
			symnode->node = NULL;
			symnode->node = tnode_create_x();
			bob_str_map_insert(&context->symtable, ident, symnode);
			parse_next_tok(context);
			parse_opt_assign(context, symnode);
		}
		else {
			report_semantics_error("Redeclaration of variable", context);
		}
		parse_opt_assign(context, symnode);
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
void parse_opt_assign(p_context_s *context, symtable_node_s *symnode) {
	if(context->currtok->type == TOK_ASSIGN) {
		tnode_s *expression;
		tok_s *op = context->currtok;
		parse_next_tok(context);
		expression = parse_expression(context);
		if (expression != NULL && symnode != NULL) {
			symnode->node = expression;
			symnode->evalflag = true;
		}
	}
}

/* 
 * function:	parse_type	
 * -------------------------------------------------- 
 */
tnode_s *parse_type(p_context_s *context) {
	tnode_arraytype_val_s array;

	tnode_list_init(&array.arr);
	tnode_s *type = parse_basic_type(context);
	parse_opt_array(context, &array.arr);
	if (array.arr.size > 0) {
		array.type = type->type;	
		type->type = PTYPE_ARRAY_DEC;
		type->val.atval = array;
	}
	return type;
}

/* 
 * function:	parse_basic_type	
 * -------------------------------------------------- 
 */
tnode_s *parse_basic_type(p_context_s *context) {
	tnode_s *typenode = NULL;
	p_nodetype_e type;
	switch(context->currtok->type) {
		case TOK_SHADER_DEC:
			type = PTYPE_SHADER_DEC;
			break;
		case TOK_TEXTURE_DEC:
			type = PTYPE_TEXTURE_DEC;
			break;
		case TOK_PROGRAM_DEC:
			type = PTYPE_PROGRAM_DEC;
			break;
		case TOK_MESH_DEC:
			type = PTYPE_MESH_DEC;
			break;
		case TOK_MODEL_DEC:
			type = PTYPE_MODEL_DEC;
			break;
		case TOK_INSTANCE_DEC:
			type = PTYPE_INSTANCE_DEC;
			break;
		case TOK_INT_DEC:
			type = PTYPE_INT_DEC;
			break;
		case TOK_FLOAT_DEC:
			type = PTYPE_FLOAT_DEC;
			break;
		case TOK_DICT_DEC:
			type = PTYPE_DICT_DEC;
			break;
		case TOK_STRING_DEC:
			type = PTYPE_STRING_DEC;
			break;
		case TOK_GENERIC_DEC:
			type = PTYPE_GENERIC_DEC;
			break;
		default:
				report_syntax_error(
					"Expected 'Shader', 'Texture', 'Program', 'Model', 'Instance', 'Num', 'Mesh', 'Dict', '*', or 'String'", context);
			parse_next_tok(context);
			return NULL;
	}
	typenode = tnode_create_basic_type(context->currtok->lineno, type);
	parse_next_tok(context);
	return typenode;
}

/* 
 * function:	parse_opt_array	
 * -------------------------------------------------- 
 */
void parse_opt_array(p_context_s *context, tnode_list_s *arr) {
	if(context->currtok->type == TOK_LBRACK) {
		tok_s *op = context->currtok;
		parse_next_tok(context);
		switch(context->currtok->type) {
			case TOK_ADDOP:
			case TOK_IDENTIFIER:
			case TOK_INTEGER:
			case TOK_FLOAT:
			case TOK_LPAREN:
			case TOK_LBRACE:
			case TOK_LBRACK: {
					tnode_s *expression = parse_expression(context);
					tnode_list_add(arr, expression);
				}
				break;
			default:
				tnode_list_add(arr, NULL);
				break;
		}
		if(context->currtok->type == TOK_RBRACK) {
			parse_next_tok(context);
		}
		else {
			report_syntax_error("Expected ']'", context);
			parse_next_tok(context);
		}
		parse_opt_array(context, arr);
	}
}

/* 
 * function:	parse_assignstmt	
 * -------------------------------------------------- 
 */
tnode_s *parse_assignstmt(p_context_s *context) {
	symtable_node_s *symnode;
	tnode_s *result = NULL, *ident;
	tok_s *val = context->currtok;
	symnode = bob_str_map_get(&context->symtable, val->lexeme);
	parse_next_tok(context);	
	parse_idsuffix(context, &symnode->node);
	if (!symnode) {
		report_semantics_error("Use of undeclared identifier", context);
	}
	if(context->currtok->type == TOK_ASSIGN) {
		tok_s *assign = context->currtok;
		parse_next_tok(context);
		tnode_s *expression = parse_expression(context);	
		symnode->evalflag = true;
		symnode->node = expression;
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
	parse_expression_(context, root);
	return root;
}

/* 
 * function:	parse_expression_
 * -------------------------------------------------- 
 */
void parse_expression_(p_context_s *context, tnode_s *root) {
	if (context->currtok->type == TOK_ADDOP) {
		tok_s *addop = context->currtok;
		parse_next_tok(context);
		tnode_s *term = parse_term(context);
		if (!strcmp(addop->lexeme, "+")) {
			if (!exec_add(root, term)) {
				// TODO: handle error
			}
		}
		else {
			if (!exec_add(root, term)) {
				// TODO: handle error
			}
		}
		parse_expression_(context, root);
	}
}

/* 
 * function:	parse_term
 * -------------------------------------------------- 
 */
tnode_s *parse_term(p_context_s *context) {
	tnode_s *root = parse_factor(context);
	parse_term_(context, root);
	return root;
}

/* 
 * function:	parse_term_	
 * -------------------------------------------------- 
 */
void parse_term_(p_context_s *context, tnode_s *root) {
	if (context->currtok->type == TOK_MULOP) {
		tok_s *mulop = context->currtok;
		parse_next_tok(context);
		tnode_s *factor = parse_factor(context);
		if(!strcmp(mulop->lexeme, "*")) {
			if (!exec_mult(root, factor)) {
				// TODO: handle error
			}
		}
		else {
			if (!exec_div(root, factor)) {
				// TODO: handle error
			}
		}
		parse_term_(context, root);
	}
}

/* 
 * function:	parse_factor	
 * -------------------------------------------------- 
 */
tnode_s *parse_factor(p_context_s *context) {
	int i; 
	double f;
	tnode_s *factor = NULL;
	switch(context->currtok->type) {
		case TOK_ADDOP: {
				tok_s *addop = context->currtok;
				parse_next_tok(context);
				tnode_s *expression = parse_expression(context);
				if(!strcmp(addop->lexeme, "-")) {
					//TODO: handle return value
					exec_negate(expression);
				}
				parse_idsuffix(context, &factor);
			}
			break;
		case TOK_IDENTIFIER:
			factor = sym_lookup(context, context->currtok->lexeme);
			if (!factor) {
				//TODO: handle error where factor is not present
			}
			parse_next_tok(context);
			parse_idsuffix(context, &factor);
			break;
		case TOK_INTEGER:
			i = atoi(context->currtok->lexeme);
			factor = tnode_create_int(context->currtok->lineno, i);
			parse_next_tok(context);
			break;
		case TOK_FLOAT:
			f = atof(context->currtok->lexeme);
			factor = tnode_create_float(context->currtok->lineno, f);
			parse_next_tok(context);
			break;
		case TOK_STRING:
			factor = tnode_create_str(context->currtok->lineno, context->currtok->lexeme);
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
				parse_next_tok(context);
				*pfactor = exec_access_obj(*pfactor, tt->lexeme);
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
					// TODO: exec function
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
					*pfactor = exec_access_arr(*pfactor, expression);
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
				tnode_list_add(&list, expression);
				parse_expression_list_(context, &list);
			}
			break;
		case TOK_IDENTIFIER:
		case TOK_INTEGER:
		case TOK_FLOAT:
		case TOK_STRING:
		case TOK_LPAREN:
		case TOK_LBRACE:
		case TOK_LBRACK:
			expression = parse_expression(context);
			tnode_list_add(&list, expression);
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
		tnode_list_add(list, expression);
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
		result = tnode_create_object(object->lineno, dict);
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
		result = tnode_create_array(array->lineno, list);
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

/* 
 * function:	exec_negate
 * TODO: type checks and error reporting
 * -------------------------------------------------- 
 */
bool exec_negate(tnode_s *operand) {
	if (operand->type == PTYPE_INT) {
		operand->val.i = -operand->val.i;
		return true;
	}
	else if (operand->type == PTYPE_FLOAT) {
		operand->val.f = -operand->val.f;
		return true;
	}
	return false;
}

/* 
 * function:	exec_sub 
 * TODO: type checks and error reporting
 * -------------------------------------------------- 
 */
bool exec_sub(tnode_s *accum, tnode_s *operand) {
	if (accum->type == PTYPE_FLOAT) {
		if (operand->type == PTYPE_FLOAT) {
			accum->val.f -= operand->val.f;
		}
		else if (operand->type == PTYPE_INT) {
			accum->val.f -= operand->val.i;
		}
		else {
			return false;
		}
	}
	else if (accum->type == PTYPE_INT) {
		if (operand->type == PTYPE_INT) {
			accum->val.i -= operand->val.i;
		}
		else if (operand->type == PTYPE_FLOAT) {
			accum->val.f = accum->val.i - operand->val.f;
			accum->type = PTYPE_FLOAT;
		}
		else {
			return false;
		}
	}
	free(operand);
	return true;
}

/* 
 * function:	exec_add
 * TODO: type checks and error reporting
 * -------------------------------------------------- 
 */
bool exec_add(tnode_s *accum, tnode_s *operand) {
	if (accum->type == PTYPE_FLOAT) {
		if (operand->type == PTYPE_FLOAT) {
			accum->val.f += operand->val.f;
		}
		else if (operand->type == PTYPE_INT) {
			accum->val.f += operand->val.i;
		}
		else if (operand->type == PTYPE_STRING) {
			char *nstr = concat_double_str(accum->val.f, operand->val.s);	
			accum->type = PTYPE_STRING;
			accum->val.s = nstr;
		}
		else {
			return false;
		}
	}
	else if (accum->type == PTYPE_INT) {
		if (operand->type == PTYPE_INT) {
			accum->val.i += operand->val.i;
		}
		else if (operand->type == PTYPE_FLOAT) {
			accum->val.f = accum->val.i + operand->val.f;
			accum->type = PTYPE_FLOAT;
		}
		else if (operand->type == PTYPE_STRING) {
			char *nstr = concat_integer_str(accum->val.i, operand->val.s);
			accum->type = PTYPE_STRING;
			accum->val.s = nstr;
		}
		else {
			return false;
		}
	}
	else if (accum->type == PTYPE_STRING) {
		if (operand->type == PTYPE_FLOAT) {
			char *nstr = concat_str_double(operand->val.s, accum->val.f);
			accum->val.s = nstr;
		}
		else if (operand->type == PTYPE_INT) {
			char *nstr = concat_str_integer(accum->val.s, operand->val.i);
			accum->val.s = nstr;
		}
		else if (operand->type == PTYPE_STRING) {
			char *nstr = concat_str_str(accum->val.s, operand->val.s);
			accum->val.s = nstr;
		}
		else {
			return false;
		}
	}
	free(operand);
	return true;
}

/* 
 * function:	exec_mult
 * TODO: type checks and error reporting
 * -------------------------------------------------- 
 */
bool exec_mult(tnode_s *accum, tnode_s *operand) {
	if (accum->type == PTYPE_FLOAT) {
		if (operand->type == PTYPE_FLOAT) {
			accum->val.f *= operand->val.f;
		}
		else if (operand->type == PTYPE_INT) {
			accum->val.f *= operand->val.i;
		}
		else {
			return false;
		}
	}
	else if (accum->type == PTYPE_INT) {
		if (operand->type == PTYPE_INT) {
			accum->val.i *= operand->val.i;
		}
		else if (operand->type == PTYPE_FLOAT) {
			accum->val.f = accum->val.i * operand->val.f;
			accum->type = PTYPE_FLOAT;
		}
		else {
			return false;
		}
	}
	free(operand);
	return true;
}

/* 
 * function:	exec_div
 * TODO: type checks and error reporting
 * -------------------------------------------------- 
 */
bool exec_div(tnode_s *accum, tnode_s *operand) {
	if (accum->type == PTYPE_FLOAT) {
		if (operand->type == PTYPE_FLOAT) {
			accum->val.f /= operand->val.f;
		}
		else if (operand->type == PTYPE_INT) {
			accum->val.f /= operand->val.i;
		}
		else {
			return false;
		}
	}
	else if (accum->type == PTYPE_INT) {
		if (operand->type == PTYPE_INT) {
			accum->val.i /= operand->val.i;
		}
		else if (operand->type == PTYPE_FLOAT) {
			accum->val.f = accum->val.i / operand->val.f;
			accum->type = PTYPE_FLOAT;
		}
		else {
			return false;
		}
	}
	free(operand);
	return true;
}

/* 
 * function:	exec_access_obj
 * -------------------------------------------------- 
 * TODO: error handling
 */
tnode_s *exec_access_obj(tnode_s *obj, char *key) {
	tnode_s *result;
	switch (obj->type) {
		case PTYPE_SHADER:
		case PTYPE_TEXTURE:
		case PTYPE_PROGRAM:
		case PTYPE_OBJECT:
			break;
		default:
			perror("type error in exec_access_obj");
			return NULL;
	}
	result = bob_str_map_get(obj->val.obj, key);
	if (!result) {
		perror("non-existant key used to access object");
	}
	return result;
}

/* 
 * function:	exec_access_arr
 * -------------------------------------------------- 
 */
tnode_s *exec_access_arr(tnode_s *arr, tnode_s *index) {
	switch (arr->type) {
		case PTYPE_ARRAY:
			if (index->type != PTYPE_INT) {
				perror("error attempt to access array with non-integer valued index");
				return NULL;
			}
			return arr->val.atval.arr.list[index->val.i];
		case PTYPE_SHADER:
		case PTYPE_TEXTURE:
		case PTYPE_PROGRAM:
		case PTYPE_OBJECT:
			if (index->type != PTYPE_STRING) {
				perror("error attempt to access object with key of wrong type");
				return NULL;
			}
			else {
				tnode_s *result;
				result = bob_str_map_get(arr->val.obj, index->val.s);
				if (!result) {
					perror("non-existant key used to access object");
				}
				return result;
			}
			break;
		default:
			perror("type error in exec_access_arr");
			return NULL;
	}
}

/* 
 * function:	sym_lookup
 * -------------------------------------------------- 
 */
tnode_s *sym_lookup(p_context_s *context, char *key) {
	return bob_str_map_get(&context->symtable, key);	
}

/* 
 * function:	concat_str_integer
 * -------------------------------------------------- 
 */
char *concat_str_integer(char *str, int i) {
	size_t n = strlen(str) + 8, realn;
	char *nstr = malloc(n);
	if (!nstr) {
		return NULL;
	}
	realn = snprintf(nstr, n, "%s%d", str, i);
	if (realn - 1 > n) {
		free(nstr);
		nstr = malloc(realn + 1);
		if (!nstr) {
			return NULL;
		}
	}
	sprintf(nstr, "%s%d", str, i);
	return nstr;
}

/* 
 * function:	concat_integer_str
 * -------------------------------------------------- 
 */
char *concat_integer_str(int i, char *str) {
	size_t n = strlen(str) + 8, realn;
	char *nstr = malloc(n);
	if (!nstr) {
		return NULL;
	}
	realn = snprintf(nstr, n, "%d%s", i, str);
	if (realn - 1 > n) {
		free(nstr);
		nstr = malloc(realn + 1);
		if (!nstr) {
			return NULL;
		}
		sprintf(nstr, "%s%d", str, i);
	}
	return nstr;
}

/* 
 * function:	concat_str_double
 * -------------------------------------------------- 
 */
char *concat_str_double(char *str, double d) {
	size_t n = strlen(str) + 8, realn;
	char *nstr = malloc(n);
	if (!nstr) {
		return NULL;
	}
	realn = snprintf(nstr, n, "%s%f", str, d);
	if (realn - 1 > n) {
		free(nstr);
		nstr = malloc(realn + 1);
		if (!nstr) {
			return NULL;
		}
	}
	sprintf(nstr, "%s%f", str, d);
	return nstr;
}

/* 
 * function:	concat_double_str
 * -------------------------------------------------- 
 */
char *concat_double_str(double d, char *str) {
	size_t n = strlen(str) + 8, realn;
	char *nstr = malloc(n);
	if (!nstr) {
		return NULL;
	}
	realn = snprintf(nstr, n, "%f%s", d, str);
	if (realn - 1 > n) {
		free(nstr);
		nstr = malloc(realn + 1);
		if (!nstr) {
			return NULL;
		}
		sprintf(nstr, "%f%s", d, str);
	}
	return nstr;
}

/* 
 * function:	concat_str
 * -------------------------------------------------- 
 */
char *concat_str_str(char *str1, char *str2) {
	size_t n = strlen(str1) + strlen(str2) + 1;
	char *nstr = malloc(n);
	if (!nstr) {
		return NULL;
	}
	sprintf("%s%s", str1, str2);
	return nstr;
}

/* 
 * function:	tnode_create_x
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_x(void) {
	tnode_s *t = calloc(sizeof *t, 1);
	if (!t) {
		return NULL;
	}
	return t;
}

/* 
 * function:	tnode_create_str
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_str(unsigned lineno, char *s) {
	tnode_s *t = malloc(sizeof *t);	
	if (!t) {
		return NULL;
	}
	t->type = PTYPE_STRING;
	t->lineno = lineno;
	t->val.s = s;
	return t;
}

/* 
 * function:	tnode_create_int
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_int(unsigned lineno, int i) {
	tnode_s *t = malloc(sizeof *t);	
	if (!t) {
		return NULL;
	}
	t->type = PTYPE_INT;
	t->lineno = lineno;
	t->val.i = i;
	return t;
}

/* 
 * function:	tnode_create_float
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_float(unsigned lineno, double f) {
	tnode_s *t = malloc(sizeof *t);	
	if (!t) {
		return NULL;
	}
	t->type = PTYPE_FLOAT;
	t->lineno = lineno;
	t->val.f = f;
	return t;
}

/* 
 * function:	tnode_create_object
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_object(unsigned lineno, StrMap *obj) {
	tnode_s *t = malloc(sizeof *t);	
	if (!t) {
		return NULL;
	}
	t->type = PTYPE_OBJECT;
	t->lineno = lineno;
	t->val.obj = obj;
	return t;
}

/* 
 * function:	tnode_create_array
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_array(unsigned lineno, tnode_list_s list) {
	tnode_s *t = malloc(sizeof *t);	
	if (!t) {
		return NULL;
	}
	t->type = PTYPE_ARRAY;
	t->lineno = lineno;
	t->val.list = list;
	return t;
}

/* 
 * function:	tnode_create_basic_type
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_basic_type(unsigned lineno, p_nodetype_e type) {
	tnode_s *t = malloc(sizeof *t);	
	if (!t) {
		return NULL;
	}
	t->type = type;
	t->lineno = lineno;
	return t;
}

/* 
 * function:	tnode_create_array_type
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_array_type(unsigned lineno, tnode_arraytype_val_s atval) {
	tnode_s *t = malloc(sizeof *t);
	t->type = PTYPE_ARRAY_DEC;
	t->lineno = lineno;
	t->val.atval = atval;
	return t;
}

/* 
 * function:	tnode_list_init
 * -------------------------------------------------- 
 */
void tnode_list_init(tnode_list_s *list) {
	list->size = 0;
	list->list = NULL;
}

/* 
 * function:	tnode_list_add
 * -------------------------------------------------- 
 */
int tnode_list_add(tnode_list_s *list, tnode_s *node) {
	list->size++;
	list->list = realloc(list->list, sizeof(*list->list) * list->size);
	if (!list->list) {
		perror("Memory allocation error with realloc().");
		return -1;
	}
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


