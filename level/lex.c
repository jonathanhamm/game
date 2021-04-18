#include "lex.h"
//#include "error.h"
#include "../common/errcodes.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add_token(toklist_s *list, char *lexeme, size_t len, unsigned lineno, toktype_e type);
static int tok_keyword(char *ptr);
static void report_lexical_error(const char *message, char c, unsigned lineno);

toklist_s lex(const char *file_name) {
	int result;
	unsigned lineno = 1;
	char *fptr, *bptr, bck;
	CharBuf src;
	toklist_s list = {NULL, NULL};

	result = char_buf_from_file(&src, file_name);
	if (result != STATUS_OK) {
		fprintf(stderr, "Error Reading file: %s\n", file_name);
		return (toklist_s) { NULL, NULL};
	}
	fptr = src.buffer;
	while (*fptr) {
		switch (*fptr) {
			case '\n':
				lineno++;
			case ' ':
			case '\t':
			case '\v':
			case '\r':
				fptr++;
				break;
			case '(':
				add_token(&list, "(", 1, lineno, TOK_LPAREN);
				fptr++;
				break;
			case ')':
				add_token(&list, ")", 1, lineno, TOK_RPAREN);
				fptr++;
				break;
			case '{':
				add_token(&list, "{", 1, lineno, TOK_LBRACE);
				fptr++;
				break;
			case '}':
				add_token(&list, "}", 1, lineno, TOK_RBRACE);
				fptr++;
				break;
			case '[':
				add_token(&list, "[", 1, lineno, TOK_LBRACK);
				fptr++;
				break;
			case ']':
				add_token(&list, "]", 1, lineno, TOK_RBRACK);
				fptr++;
				break;
			case ',':
				add_token(&list, ",", 1, lineno, TOK_COMMA);
				fptr++;
				break;
			case ':':
				if (*(fptr + 1) == '=') {
					add_token(&list, ":=", 2, lineno, TOK_ASSIGN);
					fptr += 2;
				}
				else {
					add_token(&list, ":", 1, lineno, TOK_COLON);
					fptr++;
				}
				break;
			case ';':
				add_token(&list, ";", 1, lineno, TOK_SEMICOLON);
				fptr++;
				break;
			case '.':
				add_token(&list, ".", 1, lineno, TOK_DOT);
				fptr++;
				break;
			case '+':
				add_token(&list, "+", 1, lineno, TOK_ADDOP);
				fptr++;
				break;
			case '-':
				add_token(&list, "-", 1, lineno, TOK_ADDOP);
				fptr++;
				break;
			case '*':
				add_token(&list, "*", 1, lineno, TOK_MULOP);
				fptr++;
				break;
			case '/':
				if(*(fptr + 1) == '/') {
					fptr++;
					while(*fptr && *fptr != '\n')
						fptr++;	
				}
				else if(*(fptr + 1) == '*') {
					while(*fptr) {
						if(*fptr == '*' && *(fptr + 1) == '/') {
							fptr += 2;
							break;
						}
            else if (*fptr == '\n') {
              lineno++;
            }
						fptr++;
					}
				}
				else {
					add_token(&list, "/", 1, lineno, TOK_MULOP);
					fptr++;
				}
				break;
			case '$':
				add_token(&list, "$", 1, lineno, TOK_GENERIC_DEC);
				fptr++;
				break;
			case '"':
				bptr = fptr;
				fptr++;
				while (*fptr && *fptr != '"') {
					if (*fptr == '\\') {
            char c = *(fptr + 1);
						if (c) {
              if (c == '\n')
                lineno++;
							fptr += 2;
            }
						else
							fptr++;
					}
					else {
            if (*fptr == '\n')
              lineno++;
						fptr++;
					}
				}
				if (*fptr) {
					fptr++;
					bck = *fptr;
					*fptr = '\0';
					add_token(&list, bptr, fptr - bptr, lineno, TOK_STRING);
					*fptr = bck;
				}
				else {
					report_lexical_error("unterminated string literal", '"', lineno);
				}
				break;
			default:
				if (isalpha(*fptr)) {
					toktype_e type;
					bptr = fptr;
					while (isalnum(*++fptr));
					bck = *fptr;
					*fptr = '\0';
					type = tok_keyword(bptr);
					if (type == TOK_NONE)
						add_token(&list, bptr, fptr - bptr, lineno, TOK_IDENTIFIER);
					else
						add_token(&list, bptr, fptr - bptr, lineno, type);
					*fptr = bck;
				}
				else if(isdigit(*fptr)) {
					bptr = fptr;
					while (isdigit(*++fptr));
					if (*fptr == '.') {
						fptr++;
						if (isdigit(*fptr)) {
							while (isdigit(*++fptr));	
              if (*fptr == 'e' || *fptr == 'E') {
                fptr++;
                if (*fptr == '-' || *fptr == '+')
                  fptr++;
                while(isdigit(*++fptr));
              }
							bck = *fptr;	
							*fptr = '\0';
							add_token(&list, bptr, fptr - bptr, lineno, TOK_FLOAT);
							*fptr = bck;
						}
						else {
							report_lexical_error("invalid number form", *fptr, lineno);
							fptr++;
						}
					} else if (*fptr == 'e' || *fptr == 'E') {
            fptr++;
            if (*fptr == '-' || *fptr == '+')
              fptr++;
            if (isdigit(*fptr)) {
							while (isdigit(*++fptr));	
							bck = *fptr;	
							*fptr = '\0';
							add_token(&list, bptr, fptr - bptr, lineno, TOK_FLOAT);
							*fptr = bck;
            }
          }
					else {
						bck = *fptr;	
						*fptr = '\0';
						add_token(&list, bptr, fptr - bptr, lineno, TOK_INTEGER);
						*fptr = bck;
					}
				}
				else {
					report_lexical_error("invalid character", *fptr, lineno);
					fptr++;
				}
				break;
		}
	}
	add_token(&list, "%", 1, lineno, TOK_EOF);
	return list;
}

void add_token(toklist_s *list, char *lexeme, size_t len, unsigned lineno, toktype_e type) {
	tok_s *t = malloc(sizeof(*t) + len + 1);
	if (!t) {
		perror("failure on malloc in add_token()");
		exit(EXIT_FAILURE);
	}
	t->type = type;
	t->lineno = lineno;
	t->next = NULL;
	strcpy(t->lexeme, lexeme);
	if (!list->head) 
		list->head = t;
	else
		list->tail->next = t;
	list->tail = t;
}

int tok_keyword(char *ptr) {
	if(!strcmp(ptr, "Shader"))
		return TOK_SHADER_DEC;
	else if(!strcmp(ptr, "Texture"))
		return TOK_TEXTURE_DEC;
	else if(!strcmp(ptr, "Program"))
		return TOK_PROGRAM_DEC;
	else if(!strcmp(ptr, "Model"))
		return TOK_MODEL_DEC;
	else if(!strcmp(ptr, "Mesh"))
		return TOK_MESH_DEC;
	else if(!strcmp(ptr, "Instance"))
		return TOK_INSTANCE_DEC;
  else if (!strcmp(ptr, "InstancePlane"))
    return TOK_INSTANCE_PLANE_DEC;
	else if(!strcmp(ptr, "Level"))
		return TOK_LEVEL_DEC;
	else if(!strcmp(ptr, "Float"))
		return TOK_FLOAT_DEC;
	else if(!strcmp(ptr, "Int"))
		return TOK_INT_DEC;
	else if(!strcmp(ptr, "String"))
		return TOK_STRING_DEC;
	else if(!strcmp(ptr, "Dict"))
		return TOK_DICT_DEC;
	else
		return TOK_NONE;
}

void toklist_print(toklist_s *list) {
	tok_s *t = list->head;

	while (t) {
		printf("token: %s at line %u of type %d\n", t->lexeme, t->lineno, t->type);
		t = t->next;
	}
}

void report_lexical_error(const char *message, char c, unsigned lineno) {
	fprintf(stderr, "Lexical Error at line %u, character '%c': %s\n", lineno, c, message);
}

