#include "parse.h"
#include "error.h"
#include <string.h>
#include "../common/data-structures.h"

typedef enum p_type_e p_type_e;
typedef struct p_valtype_s p_valtype_s;
typedef struct p_numlist_s p_numlist_s;
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
	PTYPE_ANY
};

struct p_valtype_s {
	p_type_e basic_type;	
	p_numlist_s *numlist;
};

struct p_numlist_s {
	int size;
	int buf[];
};

struct p_expression_s {
	p_valtype_s type;
};

struct p_term_s {
	p_valtype_s type;
};

struct p_factor_s {
	p_valtype_s type;
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
static p_numlist_s *p_numlist_create();
static void *p_numlist_add(p_numlist_s **list, int n);

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
		p_expression_s *exp;
		parse_next_tok(t);
		exp = parse_opt_assign(t);
		bob_str_map_insert(&symtable, ident, exp); 
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
		case TOK_ADDOP:
			if(!strcmp((*t)->lexeme, "+") || !strcmp((*t)->lexeme, "-")) {
				parse_next_tok(t);
				parse_expression(t);
			}
			else {
				report_syntax_error(
				"Expected identifier, number, string, '+', '-', '(', '{', or '['", *t);
				parse_next_tok(t);
			}
			break;
		case TOK_IDENTIFIER:
			parse_next_tok(t);
			parse_idsuffix(t);
			break;
		case TOK_NUMBER:
			factor = p_factor_init(PTYPE_NUM);
			parse_next_tok(t);
			break;
		case TOK_STRING:
			parse_next_tok(t);
			break;
		case TOK_LPAREN:
			parse_next_tok(t);
			parse_expression(t);
			if((*t)->type != TOK_RPAREN) {
				report_syntax_error("Expected ')'", *t);
			}
			parse_next_tok(t);
			break;
		case TOK_LBRACE:
			parse_object(t);
			break;
		case TOK_LBRACK:
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
	f->type.numlist = p_numlist_create();
	if(!f->type.numlist) {
		perror("memory allocation error in p_factor_init()");
		free(f);
		return NULL;
	}
	return f;
}

p_numlist_s *p_numlist_create() {
	p_numlist_s *l = calloc(sizeof(p_numlist_s), 1);
	if(!l) {
		perror("memory allocation error on calloc() in p_numlist_create()");
		return NULL;
	}
	return l;
}

void *p_numlist_add(p_numlist_s **list, int n) {
	p_numlist_s *original = *list;
	original->size++;
	*list = realloc(original, sizeof(*original) * original->size);
	if(!*list) {
		perror("memory allocation error on realloc() in p_numlist_add()");
		return original;
	}
	return *list;
}

