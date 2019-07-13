#include "parse.h"
#include "error.h"
#include <string.h>

static void parse_header(tok_s **t);
static void parse_property_list(tok_s **t);
static void parse_property_list_(tok_s **t);
static void parse_property(tok_s **t);
static void parse_body(tok_s **t);
static void parse_statement_list(tok_s **t);
static void parse_statement(tok_s **t);
static void parse_declaration(tok_s **t);
static void parse_type(tok_s **t);
static void parse_expression(tok_s **t);
static void parse_expression_(tok_s **t);
static void parse_term(tok_s **t);
static void parse_term_(tok_s **t);
static void parse_factor(tok_s **t);
static void parse_idsuffix(tok_s **t);
static void parse_expression_list(tok_s **t);
static void parse_expression_list_(tok_s **t);
static void parse_object(tok_s **t);
static void parse_array(tok_s **t);
static void parse_next_tok(tok_s **t);

void parse(toklist_s *list) {
	tok_s *t = list->head;

	parse_header(&t);
	parse_body(&t);
	if (t->type != TOK_EOF) {
		report_syntax_error("Expected end of source file", t);
	}
}


void parse_header(tok_s **t) {
	parse_object(t);
}

void parse_property_list(tok_s **t) {
	parse_property(t);
	parse_property_list_(t);
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
		case TOK_MODEL_DEC:
		case TOK_INSTANCE_DEC:
		case TOK_NUM_DEC:
		case TOK_STRING_DEC:
			parse_statement(t);	
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
		parse_next_tok(t);
		if ((*t)->type == TOK_ASSIGN) {
			parse_next_tok(t);
			parse_expression(t);
		}
		else {
			report_syntax_error("Expected ':='", *t);
			parse_next_tok(t);
		}
	}
	else {
		report_syntax_error("Expected identifier", *t);
		parse_next_tok(t);
	}
}

void parse_type(tok_s **t) {
	switch((*t)->type) {
		case TOK_SHADER_DEC:
		case TOK_TEXTURE_DEC:
		case TOK_PROGRAM_DEC:
		case TOK_MODEL_DEC:
		case TOK_INSTANCE_DEC:
		case TOK_NUM_DEC:
		case TOK_STRING_DEC:
			parse_next_tok(t);
			break;
		default:
			report_syntax_error(
				"Expected 'Shader', 'Texture', 'Program', 'Model', 'Instance', 'Num', or 'String'", 
				*t);
			parse_next_tok(t);
			break;
	}
}

void parse_expression(tok_s **t) {
	if((*t)->type == TOK_ADDOP 
		&& (!strcmp((*t)->lexeme, "+") || !strcmp((*t)->lexeme, "-"))) {
		parse_next_tok(t);
		parse_expression(t);
	}
	else {
		parse_term(t);
		parse_expression_(t);
	}
}

void parse_expression_(tok_s **t) {
	if ((*t)->type == TOK_ADDOP) {
		parse_next_tok(t);
		parse_term(t);
		parse_expression_(t);
	}
}

void parse_term(tok_s **t) {
	parse_factor(t);
	parse_term_(t);
}

void parse_term_(tok_s **t) {
	if ((*t)->type == TOK_MULOP) {
		parse_next_tok(t);
		parse_factor(t);
		parse_term_(t);
	}
}

void parse_factor(tok_s **t) {
	switch((*t)->type) {
		case TOK_IDENTIFIER:
			parse_next_tok(t);
			parse_idsuffix(t);
			break;
		case TOK_NUMBER:
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
			report_syntax_error("Expected identifier, number, string, '(', '{', or '['", *t);
			parse_next_tok(t);
			break;
	}
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

