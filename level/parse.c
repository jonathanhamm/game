#include "parse.h"
#include "error.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>


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
static void parse_property_list(p_context_s *context);
static void parse_property_list_(p_context_s *context);
static void parse_property(p_context_s *context);
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
static void parse_idsuffix(p_context_s *context);
static void parse_expression_list(p_context_s *context);
static void parse_expression_list_(p_context_s *context);
static tnode_s *parse_object(p_context_s *context);
static tnode_s *parse_array(p_context_s *context);
static void parse_next_tok(p_context_s *context);
static tnode_s *tnode_init(tnode_s *l, tok_s *val, p_nodetype_e type, tnode_s *r);

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

void parse_property_list(p_context_s *context) {
	if(context->currtok->type == TOK_STRING) {
		parse_property(context);
		parse_property_list_(context);
	}
}

void parse_property_list_(p_context_s *context) {
	if (context->currtok->type == TOK_COMMA) {
		parse_next_tok(context);
		parse_property(context);
		parse_property_list_(context);
	}
}

void parse_property(p_context_s *context) {
	if (context->currtok->type == TOK_STRING) {
		parse_next_tok(context);
		if (context->currtok->type == TOK_COLON) {
			parse_next_tok(context);
			parse_expression(context);
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

tnode_s *parse_expression(p_context_s *context) {
	tnode_s *root = parse_term(context);
	parse_expression_(context, &root);
	return root;
}

void parse_expression_(p_context_s *context, tnode_s **root) {
	if (context->currtok->type == TOK_ADDOP) {
		tok_s *addop = context->currtok;
		parse_next_tok(context);
		tnode_s *term = parse_term(context);
		if(!strcmp(addop->lexeme, "+"))
			*root = tnode_init(*root, addop, PTYPE_ADDITION, term); 
		else
			*root = tnode_init(*root, addop, PTYPE_SUBTRACTION, term);
		parse_next_tok(context);
		parse_expression_(context, root);
	}
}

tnode_s *parse_term(p_context_s *context) {
	tnode_s *root = parse_factor(context);
	parse_term_(context, &root);
	return root;
}

void parse_term_(p_context_s *context, tnode_s **root) {
	if (context->currtok->type == TOK_MULOP) {
		tok_s *mulop = context->currtok;
		parse_next_tok(context);
		tnode_s *factor = parse_factor(context);
		if(!strcmp(mulop->lexeme, "*"))
			*root = tnode_init(*root, mulop, PTYPE_MULTIPLICATION, factor);
		else
			*root = tnode_init(*root, mulop, PTYPE_DIVISION, factor);
		parse_term_(context, root);
	}
}

tnode_s *parse_factor(p_context_s *context) {
	tnode_s *factor;
	switch(context->currtok->type) {
		case TOK_ADDOP: {
				tok_s *addop = context->currtok;
				parse_next_tok(context);
				tnode_s *expression = parse_expression(context);
				if(!strcmp(addop->lexeme, "-"))
					factor = tnode_init(expression, addop, PTYPE_NEGATION, NULL);
				else
					factor = tnode_init(expression, addop, PTYPE_POS, NULL);
			}
			break;
		case TOK_IDENTIFIER:
			factor = tnode_init(NULL, context->currtok, PTYPE_VAL, NULL);
			parse_next_tok(context);
			parse_idsuffix(context);
			break;
		case TOK_NUMBER:
			factor = tnode_init(NULL, context->currtok, PTYPE_VAL, NULL);
			parse_next_tok(context);
			break;
		case TOK_STRING:
			factor = tnode_init(NULL, context->currtok, PTYPE_VAL, NULL);
			parse_next_tok(context);
			break;
		case TOK_LPAREN: {
				parse_next_tok(context);
				factor = parse_expression(context);
				if(context->currtok->type != TOK_RPAREN) {
					report_syntax_error("Expected ')'", context);
				}
				parse_next_tok(context);
			}
			break;
		case TOK_LBRACE:
			factor = parse_object(context);
			break;
		case TOK_LBRACK:
			factor = parse_array(context);
			break;
		default:
			report_syntax_error("Expected identifier, number, string, '+', '-', '(', '{', or '['", context);
			parse_next_tok(context);
			break;
	}
	return NULL;
}

void parse_idsuffix(p_context_s *context) {
	switch(context->currtok->type) {
		case TOK_DOT:
			parse_next_tok(context);
			if(context->currtok->type == TOK_IDENTIFIER) {
				tok_s *tt = context->currtok;
				parse_next_tok(context);
				parse_idsuffix(context);
			}
			else {
				report_syntax_error("Expected identifier", context);
				parse_next_tok(context);
			}
			break;
		case TOK_LPAREN:
			parse_next_tok(context);
			parse_expression_list(context);
			if(context->currtok->type == TOK_RPAREN) {
				parse_next_tok(context);
				parse_idsuffix(context);
			}
			else {
				report_syntax_error("Expected ')'", context);
				parse_next_tok(context);
			}
			break;
		case TOK_LBRACK:
			parse_next_tok(context);
			parse_expression(context);
			if(context->currtok->type == TOK_RBRACK) {
				parse_next_tok(context);
				parse_idsuffix(context);
			}
			else {
				report_syntax_error("Expected ']'", context);
				parse_next_tok(context);
			}
			break;
		default:
			break;
	}
}

void parse_expression_list(p_context_s *context) {
	switch(context->currtok->type) {
		case TOK_ADDOP:
			if(!strcmp(context->currtok->lexeme, "+") || !strcmp(context->currtok->lexeme, "-")) {
				parse_expression(context);
				parse_expression_list_(context);
			}
			break;
		case TOK_IDENTIFIER:
		case TOK_NUMBER:
		case TOK_STRING:
		case TOK_LPAREN:
		case TOK_LBRACE:
		case TOK_LBRACK:
			parse_expression(context);
			parse_expression_list_(context);
			break;
		default:
			break;
	}
}

void parse_expression_list_(p_context_s *context) {
	if(context->currtok->type ==  TOK_COMMA) {
		parse_next_tok(context);
		parse_expression(context);
		parse_expression_list_(context);
	}
}

tnode_s *parse_object(p_context_s *context) {
	if(context->currtok->type == TOK_LBRACE) {
		parse_next_tok(context);
		parse_property_list(context);
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
}

tnode_s *parse_array(p_context_s *context) {
	if(context->currtok->type == TOK_LBRACK) {
		parse_next_tok(context);
		parse_expression_list(context);
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
}

void parse_next_tok(p_context_s *context) {
	if(context->currtok->type != TOK_EOF)
		context->currtok = context->currtok->next;
}

static tnode_s *tnode_init(tnode_s *l, tok_s *val, p_nodetype_e type, tnode_s *r) {
	tnode_s *t = malloc(sizeof *t);
	if(!t) {
		perror("Memory Error in malloc() while allocating new tnode");
		return NULL;
	}
	t->l = l;
	t->val = val;
	t->type = type;
	t->r = r;
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

