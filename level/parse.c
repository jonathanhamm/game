#include "parse.h"
#include "error.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "../common/data-structures.h"

typedef enum p_type_e p_type_e;
typedef struct p_valtype_s p_valtype_s;
typedef struct p_accessitem_s p_accessitem_s;
typedef struct p_accesslist_s p_accesslist_s;
typedef struct p_expression_s p_expression_s;
typedef struct p_term_s p_term_s;
typedef struct p_factor_s p_factor_s;

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

struct p_valtype_s {
	p_type_e basic_type;	
	p_accesslist_s *accesslist;
};

struct p_accessitem_s {
	bool isnum;
	union {
		int num;
		char *key;
	} item;
};

struct p_accesslist_s {
	int size;
	p_accessitem_s items[];
};

struct p_expression_s {
	p_valtype_s type;
};

struct p_term_s {
	p_valtype_s type;
};

struct p_factor_s {
	p_valtype_s type;
	union {
		p_expression_s *expression;
		tok_s *token;
		double num;
		char *str;
	} val;
};

static void parse_header(tok_s **t);
static void parse_property_list(tok_s **t);
static void parse_property_list_(tok_s **t);
static void parse_property(tok_s **t);
static void parse_body(tok_s **t);
static void parse_statement_list(tok_s **t);
static void parse_statement(tok_s **t);
static void parse_declaration(tok_s **t);
static p_expression_s *parse_opt_assign(tok_s **t);
static void parse_type(tok_s **t);
static void parse_basic_type(tok_s **t);
static void parse_opt_array(tok_s **t);
static p_expression_s *parse_expression(tok_s **t);
static void parse_expression_(tok_s **t);
static p_term_s *parse_term(tok_s **t);
static void parse_term_(tok_s **t, p_term_s *p);
static p_factor_s *parse_factor(tok_s **t);
static void parse_idsuffix(tok_s **t);
static void parse_expression_list(tok_s **t);
static void parse_expression_list_(tok_s **t);
static void parse_object(tok_s **t);
static void parse_array(tok_s **t);
static void parse_next_tok(tok_s **t);

static p_factor_s *p_factor_init(p_type_e basic_type);
static p_factor_s *p_factor_init_from_type(p_valtype_s *type);
static p_accesslist_s *p_accesslist_create(void);
static p_accesslist_s *p_accesslist_clone(p_accesslist_s *l);
static p_accesslist_s *p_accesslist_add_int(p_accesslist_s *l, int n);
static p_accesslist_s *p_accesslist_add_key(p_accesslist_s *l, char *key);
static void p_asslist_free(p_accesslist_s *l);

static void report_syntax_error(const char *message, tok_s *tok);
static void report_semantics_error(const char *message, tok_s *tok);

static int parse_errors;
static StrMap symtable;

void parse(toklist_s *list) {
	tok_s *t = list->head;

	bob_str_map_init(&symtable);
	parse_header(&t);
	parse_body(&t);
	if (t->type != TOK_EOF) {
		report_syntax_error("Expected end of source file", t);
	}
	bob_str_map_free(&symtable);
}

void parse_header(tok_s **t) {
	parse_object(t);
}

void parse_property_list(tok_s **t) {
	if((*t)->type == TOK_STRING) {
		parse_property(t);
		parse_property_list_(t);
	}
}

void parse_property_list_(tok_s **t) {
	if ((*t)->type == TOK_COMMA) {
		parse_next_tok(t);
		parse_property(t);
		parse_property_list_(t);
	}
}

void parse_property(tok_s **t) {
	if ((*t)->type == TOK_STRING) {
		parse_next_tok(t);
		if ((*t)->type == TOK_COLON) {
			parse_next_tok(t);
			parse_expression(t);
		}
		else {
			report_syntax_error("Expected ':'", *t);
			parse_next_tok(t);
		}
	}
	else {
		report_syntax_error("Expected string", *t);
		parse_next_tok(t);
	}
}

void parse_body(tok_s **t) {
	if ((*t)->type == TOK_LBRACE) {
		parse_next_tok(t);
		parse_statement_list(t);	
		if ((*t)->type != TOK_RBRACE) {
			report_syntax_error("Expected '}'", *t);
		}
	}
	else {
		report_syntax_error("Expected '{'", *t);
	}
	parse_next_tok(t);
}

void parse_statement_list(tok_s **t) {
	switch((*t)->type) {
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
			parse_statement(t);	
			if((*t)->type == TOK_SEMICOLON) {
				parse_next_tok(t);
			}
			parse_statement_list(t);
			break;
		default:
			break;
	}
}

void parse_statement(tok_s **t) {
	parse_declaration(t);
}

void parse_declaration(tok_s **t) {
	parse_type(t);
	if((*t)->type == TOK_IDENTIFIER) {
		char *ident = (*t)->lexeme;
		p_expression_s *expression;
		parse_next_tok(t);
		expression = parse_opt_assign(t);
		bob_str_map_insert(&symtable, ident, expression); 
	}
	else {
		report_syntax_error("Expected identifier", *t);
		parse_next_tok(t);
	}
}

p_expression_s *parse_opt_assign(tok_s **t) {
	if((*t)->type == TOK_ASSIGN) {
		parse_next_tok(t);
		return parse_expression(t);
	}
	return NULL;
}

void parse_type(tok_s **t) {
	parse_basic_type(t);
	parse_opt_array(t);
}

void parse_basic_type(tok_s **t) {
	switch((*t)->type) {
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
			parse_next_tok(t);
			break;
		default:
				report_syntax_error(
					"Expected 'Shader', 'Texture', 'Program', 'Model', 'Instance', 'Num', 'Mesh', 'Dict', '*', or 'String'", *t);
			parse_next_tok(t);
			break;
	}
}

void parse_opt_array(tok_s **t) {
	if((*t)->type == TOK_LBRACK) {
		parse_next_tok(t);
		switch((*t)->type) {
			case TOK_ADDOP:
				if(!strcmp((*t)->lexeme, "+") || !strcmp((*t)->lexeme, "-")) {
					parse_expression(t);
				}
				break;
			case TOK_IDENTIFIER:
			case TOK_NUMBER:
			case TOK_LPAREN:
			case TOK_LBRACE:
			case TOK_LBRACK:
				parse_expression(t);
				break;
			default:
				break;
		}
		if((*t)->type == TOK_RBRACK) {
			parse_next_tok(t);
		}
		else {
			report_syntax_error("Expected ']'", *t);
			parse_next_tok(t);
		}
		parse_opt_array(t);
	}
}

p_expression_s *parse_expression(tok_s **t) {
	parse_term(t);
	parse_expression_(t);
}

void parse_expression_(tok_s **t) {
	if ((*t)->type == TOK_ADDOP) {
		parse_next_tok(t);
		parse_term(t);
		parse_expression_(t);
	}
}

p_term_s *parse_term(tok_s **t) {
	p_term_s *term;
	parse_factor(t);
	parse_term_(t, term);
}

void parse_term_(tok_s **t, p_term_s *term) {
	if ((*t)->type == TOK_MULOP) {
		parse_next_tok(t);
		parse_factor(t);
		parse_term_(t, term);
	}
}

p_factor_s *parse_factor(tok_s **t) {
	p_factor_s *factor;

	switch((*t)->type) {
		case TOK_ADDOP: {
				int neg = !strcmp((*t)->lexeme, "-");
				p_expression_s *expression;
				if(neg || !strcmp((*t)->lexeme, "+")) {
					parse_next_tok(t);
					expression = parse_expression(t);
					factor = p_factor_init_from_type(&expression->type);
					factor->val.expression = expression;
				}
				else {
					report_syntax_error(
					"Expected identifier, number, string, '+', '-', '(', '{', or '['", *t);
					parse_next_tok(t);
				}
			}
			break;
		case TOK_IDENTIFIER: {
				char *ident = (*t)->lexeme;
				p_expression_s *expression;
				expression = bob_str_map_get(&symtable, ident);
				if(!expression) {
					report_semantics_error("Use of undelcared identifier", *t);		
					expression = NULL;
				}
				parse_next_tok(t);
				parse_idsuffix(t);
			}
			break;
		case TOK_NUMBER:
			factor = p_factor_init(PTYPE_NUM);
			factor->val.num = atof((*t)->lexeme);
			parse_next_tok(t);
			break;
		case TOK_STRING:
			factor = p_factor_init(PTYPE_STRING);
			factor->val.str = (*t)->lexeme;
			parse_next_tok(t);
			break;
		case TOK_LPAREN: {
				p_expression_s *expression;
				parse_next_tok(t);
				expression = parse_expression(t);
				factor = p_factor_init_from_type(&expression->type);
				factor->val.expression = expression;
				if((*t)->type != TOK_RPAREN) {
					report_syntax_error("Expected ')'", *t);
				}
				parse_next_tok(t);
			}
			break;
		case TOK_LBRACE:
			factor = p_factor_init(PTYPE_DICT);
			parse_object(t);
			break;
		case TOK_LBRACK:
			factor = p_factor_init(PTYPE_ARRAY);
			parse_array(t);
			break;
		default:
			report_syntax_error("Expected identifier, number, string, '+', '-', '(', '{', or '['", *t);
			parse_next_tok(t);
			factor = NULL;
			break;
	}
	return factor;
}

void parse_idsuffix(tok_s **t) {
	switch((*t)->type) {
		case TOK_DOT:
			parse_next_tok(t);
			if((*t)->type == TOK_IDENTIFIER) {
				parse_next_tok(t);
				parse_idsuffix(t);
			}
			else {
				report_syntax_error("Expected identifier", *t);
				parse_next_tok(t);
			}
			break;
		case TOK_LPAREN:
			parse_next_tok(t);
			parse_expression_list(t);
			if((*t)->type == TOK_RPAREN) {
				parse_next_tok(t);
				parse_idsuffix(t);
			}
			else {
				report_syntax_error("Expected ')'", *t);
				parse_next_tok(t);
			}
			break;
		case TOK_LBRACK:
			parse_next_tok(t);
			parse_expression(t);
			if((*t)->type == TOK_RBRACK) {
				parse_next_tok(t);
				parse_idsuffix(t);
			}
			else {
				report_syntax_error("Expected ']'", *t);
				parse_next_tok(t);
			}
			break;
		default:
			break;
	}
}

void parse_expression_list(tok_s **t) {
	switch((*t)->type) {
		case TOK_ADDOP:
			if(!strcmp((*t)->lexeme, "+") || !strcmp((*t)->lexeme, "-")) {
				parse_expression(t);
				parse_expression_list_(t);
			}
			break;
		case TOK_IDENTIFIER:
		case TOK_NUMBER:
		case TOK_STRING:
		case TOK_LPAREN:
		case TOK_LBRACE:
		case TOK_LBRACK:
			parse_expression(t);
			parse_expression_list_(t);
			break;
		default:
			break;
	}
}

void parse_expression_list_(tok_s **t) {
	if((*t)->type ==  TOK_COMMA) {
		parse_next_tok(t);
		parse_expression(t);
		parse_expression_list_(t);
	}
}

void parse_object(tok_s **t) {
	if((*t)->type == TOK_LBRACE) {
		parse_next_tok(t);
		parse_property_list(t);
		if((*t)->type == TOK_RBRACE) {
			parse_next_tok(t);
		}
		else {
			report_syntax_error("Expected '}'", *t);
			parse_next_tok(t);
		}
	}
	else {
		report_syntax_error("Expected '{'", *t);
		parse_next_tok(t);
	}
}

void parse_array(tok_s **t) {
	if((*t)->type == TOK_LBRACK) {
		parse_next_tok(t);
		parse_expression_list(t);
		if((*t)->type == TOK_RBRACK) {
			parse_next_tok(t);
		}
		else {
			report_syntax_error("Expected ']'", *t);
			parse_next_tok(t);
		}
	}
	else {
		report_syntax_error("Expected '['", *t);
		parse_next_tok(t);
	}
}

void parse_next_tok(tok_s **t) {
	if((*t)->type != TOK_EOF)
		*t = (*t)->next;
}

p_factor_s *p_factor_init(p_type_e basic_type) {
	p_factor_s *f = malloc(sizeof *f);
	if(!f) {
		perror("memory allocation error on malloc() in p_factor_init()");
		return NULL;
	}
	f->type.basic_type = basic_type;
	f->type.accesslist = p_accesslist_create();
	if(!f->type.accesslist) {
		perror("memory allocation error in p_factor_init()");
		free(f);
		return NULL;
	}
	return f;
}


p_factor_s *p_factor_init_from_type(p_valtype_s *t) {
	p_factor_s *f = p_factor_init(t->basic_type);
	if(f) {
		f->type.accesslist = p_accesslist_clone(t->accesslist);
		return f;
	}
	return NULL;
}

p_accesslist_s *p_accesslist_create(void) {
	p_accesslist_s *l = calloc(sizeof *l, 1);	
	if(!l) {
		perror("memory allocation error on calloc() in p_accesslist_create()");
		return NULL;
	}
	return l;
}

p_accesslist_s *p_accesslist_clone(p_accesslist_s *l) {
	int i, size = l->size;
	p_accesslist_s *ll = malloc(sizeof(*l) + sizeof(p_accessitem_s) * size);	
	if(!ll) {
		perror("memory allocation error on malloc() in p_accesslist_clone");
		return NULL;
	}
	ll->size = size;
	for(i = 0; i < size; i++)
		ll->items[i].item = l->items[i].item;
}

p_accesslist_s *p_accesslist_add_int(p_accesslist_s *l, int n) {
	p_accesslist_s *ll = realloc(l, sizeof(*l) + sizeof(p_accessitem_s) * (l->size + 1));
	if(!ll) {
		perror("memory allocation error on realloc() in p_accesslist_add_int");
		return l;
	}
	ll->items[ll->size].isnum = true;
	ll->items[ll->size].item.num = n;
	ll->size++;
	return ll;
}

p_accesslist_s *p_accesslist_add_key(p_accesslist_s *l, char *key) {
	p_accesslist_s *ll = realloc(l, sizeof(*l) + sizeof(p_accessitem_s) * (l->size + 1));
	if(!ll) {
		perror("memory allocation error on realloc() in p_accesslist_add_key");
		return l;
	}
	ll->items[ll->size].isnum = false;
	ll->items[ll->size].item.key = key;
	ll->size++;
	return ll;
}

static void p_asslist_free(p_accesslist_s *l) {
	free(l);
}

void report_syntax_error(const char *message, tok_s *tok) {
	parse_errors++;
	fprintf(stderr, "Syntax Error at line %u, token: '%s': %s\n", tok->lineno, tok->lexeme, message);
}

void report_semantics_error(const char *message, tok_s *tok) {
	parse_errors++;
	fprintf(stderr, "Error at line %u, token '%s': %s\n", tok->lineno, tok->lexeme, message);
}

