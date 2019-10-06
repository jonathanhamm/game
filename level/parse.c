#include "parse.h"
//#include "error.h"
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
static void parse_opt_assign(p_context_s *context, tnode_s **pident);
static tnode_s *parse_type(p_context_s *context);
static tnode_s *parse_basic_type(p_context_s *context);
static void parse_opt_array(p_context_s *context, tnode_s **ptype);
static tnode_s *parse_assignstmt(p_context_s *context);
static tnode_s *parse_expression(p_context_s *context);
static void parse_expression_(p_context_s *context, tnode_s *root);
static tnode_s *parse_term(p_context_s *context);
static void parse_term_(p_context_s *context, tnode_s **root);
static tnode_s *parse_factor(p_context_s *context);
static void parse_idsuffix(p_context_s *context, tnode_s **pfactor);
static tnode_list_s parse_expression_list(p_context_s *context);
static void parse_expression_list_(p_context_s *context, tnode_list_s *list);
static tnode_s *parse_object(p_context_s *context);
static tnode_s *parse_array(p_context_s *context);
static void parse_next_tok(p_context_s *context);

static bool exec_sub(tnode_s *accum, tnode_s *operand);
static bool exec_add(tnode_s *accum, tnode_s *operand);
static bool exec_mult(tnode_s *accum, tnode_s *operand);
static bool exec_div(tnode_s *accum, tnode_s *operand);
static char *concat_str_integer(int i, char *str);
static char *concat_integer_str(int i, char *str);
static char *concat_str_double(char *str, double d);
static char *concat_double_str(double d, char *str);
static char *concat_str_str(char *str1, char *str2);
static tnode_s *tnode_create_str(p_notetype_e type, unsigned lineno, char *s);
static tnode_s *tnode_create_int(p_notetype_e type, unsigned lineno, int i);
static tnode_s *tnode_create_float(p_notetype_e type, unsigned lineno, double f);
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
	tnode_s *result = NULL, *typenode;
	tok_s *typetok = context->currtok;
	typenode = parse_type(context);
	if(context->currtok->type == TOK_IDENTIFIER) {
		tok_s *identtok = context->currtok;
		char *ident = identtok->lexeme;
		symtable_node_s *symnode = malloc(sizeof *symnode);
		if(!symnode) {
			perror("Memory Allocation error in malloc while allocating a symtable node.");
			return NULL;
		}
		symnode->evalflag = false;
		symnode->node = NULL;
		tnode_s	*dectarget = tnode_init_val(identtok);
		bob_str_map_insert(&context->symtable, ident, symnode);
		parse_next_tok(context);
		parse_opt_assign(context, &dectarget);
		result = tnode_init_bin(typenode, typetok, PTYPE_DEC, dectarget);
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
		case TOK_INT_DEC:
		case TOK_FLOAT_DEC:
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
			case TOK_INTEGER:
			case TOK_FLOAT:
			case TOK_LPAREN:
			case TOK_LBRACE:
			case TOK_LBRACK: {
					tnode_s *expression = parse_expression(context);
					*ptype = tnode_init_bin(*ptype, op, PTYPE_ARRAY_DEC, expression);
				}
				break;
			default: {
				tnode_s *narray = calloc(1, sizeof *narray);
				if(narray) {
					narray->type = PTYPE_ANY;
					*ptype = tnode_init_bin(*ptype, op, PTYPE_ARRAY_DEC, narray);
				}
				else {
					perror("Memory allocation error: calloc() in parse_opt_array() creating new tnode");
				}
			}
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
void parse_expression_(p_context_s *context, tnode_s *root) {
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
	int i, double f;
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
		case TOK_INTEGER:
			i = atoi(context->currtok->lexeme);
			factor = tnode_create_int(PTYPE_INT, context->currtok->lineno, i);
			parse_next_tok(context);
			break;
		case TOK_FLOAT:
			f = atof(context->currtok->lexeme);
			factor = tnode_create_float(PTYPE_FLOAT, context->currtok->lineno, f);
			parse_next_tok(context);
			break;
		case TOK_STRING:
			factor = tnode_create_str(PTYPE_STRING, context->currtok->lineno, context->currtok->lexeme);
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
		case TOK_INTEGER:
		case TOK_FLOAT:
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
		else if (operand->type == PTYPE_INTEGER) {
			accum->val.f -= operand->val.i;
		}
		else {
			return false;
		}
	}
	else if (accum->type == PTYPE_INTEGER) {
		if (operand->type == PTYPE_INTEGER) {
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
		else if (operand->type == PTYPE_INTEGER) {
			accum->val.f += operand->val.i;
		}
		else if (operand->type == PTYPE_STRING) {
			char *nstr = concat_double_str(accum->val.f, operand->val.s);	
			accum->type = PTYPE_STRING;
			accum->val.str = nstr;
		}
		else {
			return false;
		}
	}
	else if (accum->type == PTYPE_INTEGER) {
		if (operand->type == PTYPE_INTEGER) {
			accum->val.i += operand->val.i;
		}
		else if (operand->type == PTYPE_FLOAT) {
			accum->val.f = accum->val.i + operand->val.f;
			accum->type = PTYPE_FLOAT;
		}
		else if (operand->type == PTYPE_STRING) {
			char *nstr = concat_integer_str(accum->val.i, operand->val.s);
			accum->type = PTYPE_STRING;
			accum->val.str = nstr;
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
		else if (operand->type == PTYPE_INTEGER) {
			char *nstr = concat_str_integer(accum->val.s, operand->val.f);
			accum->val.s = = nstr;
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
		else if (operand->type == PTYPE_INTEGER) {
			accum->val.f *= operand->val.i;
		}
		else {
			return false;
		}
	}
	else if (accum->type == PTYPE_INTEGER) {
		if (operand->type == PTYPE_INTEGER) {
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
		else if (operand->type == PTYPE_INTEGER) {
			accum->val.f /= operand->val.i;
		}
		else {
			return false;
		}
	}
	else if (accum->type == PTYPE_INTEGER) {
		if (operand->type == PTYPE_INTEGER) {
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
 * function:	concat_str_integer
 * -------------------------------------------------- 
 */
char *concat_str_integer(int i, char *str) {
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
	realn = snprintf(nstr, n, "%d%s", str, i);
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
	sprintf(nstr, "%s%d", str, d);
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
	realn = snprintf(nstr, n, "%f%s", str, d);
	if (realn - 1 > n) {
		free(nstr);
		nstr = malloc(realn + 1);
		if (!nstr) {
			return NULL;
		}
		sprintf(nstr, "%s%d", str, d);
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
 * function:	tnode_create_str
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_str(p_notetype_e type, unsigned lineno, char *s) {
	tnode_s *t = malloc(sizeof *t);	
	if (!t) {
		return NULL;
	}
	t->type = type;
	t->lineno = lineno;
	t->val.s = s;
	return t;
}

/* 
 * function:	tnode_create_int
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_int(p_notetype_e type, unsigned lineno, int i) {
	tnode_s *t = malloc(sizeof *t);	
	if (!t) {
		return NULL;
	}
	t->type = type;
	t->lineno = lineno;
	t->val.i = i;
	return t;
}

/* 
 * function:	tnode_create_float
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_float(p_notetype_e type, unsigned lineno, double f) {
	tnode_s *t = malloc(sizeof *t);	
	if (!t) {
		return NULL;
	}
	t->type = type;
	t->lineno = lineno;
	t->val.f = f;
	return t;
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


