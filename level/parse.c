#include "parse.h"
#include "../common/constants.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define M_KEY(k) ("\""k"\"")
#define OBJ_ID_KEY "__name__"
#define OBJ_ISGEN_KEY "__isgen__"

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

static const bool isgen_false = false;
static const bool isgen_true = true;

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
static bool typecheck_assignment(p_context_s *context, tnode_s *typenode, tnode_s *expression);
static bool typecheck_basic_assignment(p_context_s *context, p_nodetype_e declared, p_nodetype_e expression);
static bool typecheck_array_assignment(p_context_s *context, tnode_s *typenode, tnode_s *expression);
static p_nodetype_e resolve_array_type(tnode_list_s list);
static bool exec_negate(tnode_s *operand);
static bool exec_sub(tnode_s *accum, tnode_s *operand);
static bool exec_add(tnode_s *accum, tnode_s *operand);
static bool exec_mult(tnode_s *accum, tnode_s *operand);
static bool exec_div(tnode_s *accum, tnode_s *operand);
static tnode_s *exec_access_obj(tnode_s *obj, char *key);
static tnode_s *exec_access_arr(tnode_s *arr, tnode_s *index);
static symtable_node_s *sym_lookup(p_context_s *context, char *key);
static char *concat_str_integer(char *str, int i);
static char *concat_integer_str(int i, char *str);
static char *concat_str_double(char *str, double d);
static char *concat_double_str(double d, char *str);
static char *concat_str_str(char *str1, char *str2);
static tnode_s *tnode_create_x(void);
static tnode_s *tnode_create_str(tok_s *tok, char *s);
static tnode_s *tnode_create_int(tok_s *tok, int i);
static tnode_s *tnode_create_float(tok_s *tok, double f);
static tnode_s *tnode_create_object(tok_s *tok, StrMap *obj);
static tnode_s *tnode_create_array(tok_s *tok, tnode_list_s list, p_nodetype_e type);
static tnode_s *tnode_create_basic_type(tok_s *tok, p_nodetype_e type);
static tnode_s *tnode_create_array_type(tok_s *tok, tnode_arraytype_val_s atval);
static char *make_label(p_context_s *context, char *prefix);
static void tnode_list_init(tnode_list_s *list);
static int tnode_list_add(tnode_list_s *list, tnode_s *node);
static void emit_code(const char *code, CharBuf *segment);
static void emit_instance_batch(p_context_s *context, char *levelid, tnode_list_s instances);
static void emit_instance_plane_batch(p_context_s *context, char *levelid, tnode_list_s instances);
static bool emit_instance(p_context_s *context, char *levelid, tnode_s *instance);
static bool emit_level(p_context_s *context, tnode_s *level);
static bool emit_instance_data(p_context_s *context, tnode_s *level, char *levelid);
static bool emit_instances(p_context_s *context, char *levelid, tnode_s *instances);
static bool emit_instance_planes(p_context_s *context, char *levelid, tnode_s *instance_planes);
static bool emit_instance_plane(p_context_s *context, char *levelid, tnode_s *instance_plane);
static bool emit_model(tnode_s *model, p_context_s *context);
static bool emit_mesh(tnode_s *mesh, p_context_s *context);
static bool emit_program(tnode_s *program, p_context_s *context);
static bool emit_shader(tnode_s *shader, bob_shader_e type, p_context_s *context);
static bool emit_texture(tnode_s *texture, p_context_s *context);
static bool check_shader(tnode_s *program, p_context_s *context, const char *shader, const char *programname, bob_shader_e type);
static CharBuf val_to_str(tnode_s *val);
static char *strip_quotes(char *level_name);
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
  context.labelcount = 0;
  char_buf_init(&context.meshcode);
  char_buf_init(&context.shadercode);
  char_buf_init(&context.programcode);
  char_buf_init(&context.texturecode);
  char_buf_init(&context.modelcode);
  char_buf_init(&context.levelcode);
  char_buf_init(&context.instancecode);
  char_buf_init(&context.instanceplanecode);
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
    case TOK_INSTANCE_PLANE_DEC:
    case TOK_LEVEL_DEC:
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
      if (symnode->evalflag) {
        bool result = typecheck_assignment(context, typenode, symnode->node);
        if (!result) {
          report_semantics_error("Type Error", context);
        }
        else if(typenode->type == PTYPE_LEVEL_DEC) {
          emit_level(context, symnode->node);				
        }
        else {
          //printf("typenode type: %d\n", typenode->type);
        }
      }
    }
    else {
      report_semantics_error("Redeclaration of variable", context);
    }
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
    case TOK_INSTANCE_PLANE_DEC:
      type = PTYPE_INSTANCE_PLANE_DEC;
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
    case TOK_LEVEL_DEC:
      type = PTYPE_LEVEL_DEC;
      break;
    default:
      report_syntax_error(
          "Expected 'Shader', 'Texture', 'Program', 'Model', 'Instance', 'Num', 'Mesh', 'Dict', '*', or 'String'", context);
      parse_next_tok(context);
      return NULL;
  }
  typenode = tnode_create_basic_type(context->currtok, type);
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
        break;
      }
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
    result = parse_expression(context);	
    if (symnode != NULL) {
      symnode->evalflag = true;
      symnode->node = result;
    }
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
  symtable_node_s *symnode;
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
        factor = expression;
        parse_idsuffix(context, &factor);
      }
      break;
    case TOK_IDENTIFIER:
      symnode = sym_lookup(context, context->currtok->lexeme);
      if (!symnode) {
        //TODO: handle error where factor is not present
        report_semantics_error("Use of undeclared identifier", context);
      }
      if (!symnode->evalflag) {
        report_semantics_error("Use of unitialized identifier", context);
      }
      factor = symnode->node;
      parse_next_tok(context);
      parse_idsuffix(context, &factor);
      break;
    case TOK_INTEGER:
      i = atoi(context->currtok->lexeme);
      factor = tnode_create_int(context->currtok, i);
      parse_next_tok(context);
      break;
    case TOK_FLOAT:
      f = atof(context->currtok->lexeme);
      factor = tnode_create_float(context->currtok, f);
      parse_next_tok(context);
      break;
    case TOK_STRING:
      factor = tnode_create_str(context->currtok, context->currtok->lexeme);
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
        if (*pfactor) {
          *pfactor = exec_access_arr(*pfactor, expression);
        }
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
    result = tnode_create_object(object, dict);
    char *label = make_label(context, "obj");
    bob_str_map_insert(dict, OBJ_ID_KEY, label);
    bob_str_map_insert(dict, OBJ_ISGEN_KEY, (void *)&isgen_false);
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
    p_nodetype_e type = resolve_array_type(list);
    result = tnode_create_array(array, list, type);
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

/* 
 * function: parse_next_tok
 * -------------------------------------------------- 
 */
void parse_next_tok(p_context_s *context) {
  if(context->currtok->type != TOK_EOF)
    context->currtok = context->currtok->next;
}

/* 
 * function: typecheck_assignment
 * -------------------------------------------------- 
 */
bool typecheck_assignment(p_context_s *context, tnode_s *typenode, tnode_s *expression) {
  if (typenode->type == PTYPE_ARRAY_DEC)
    return typecheck_array_assignment(context, typenode, expression);
  else 
    return typecheck_basic_assignment(context, typenode->type, expression->type);
}

/* 
 * function: typecheck_basic_assignment
 * -------------------------------------------------- 
 */
bool typecheck_basic_assignment(p_context_s *context, p_nodetype_e declared, p_nodetype_e expression) {
  switch (declared) {
    case PTYPE_SHADER_DEC:
      return expression == PTYPE_OBJECT || expression == PTYPE_SHADER;	
    case PTYPE_TEXTURE_DEC:
      return expression == PTYPE_OBJECT || expression == PTYPE_TEXTURE;
    case PTYPE_PROGRAM_DEC:
      return expression == PTYPE_OBJECT || expression == PTYPE_PROGRAM;
    case PTYPE_MESH_DEC:
      return expression == PTYPE_OBJECT || expression == PTYPE_MESH;
    case PTYPE_MODEL_DEC:
      return expression == PTYPE_OBJECT || expression == PTYPE_MODEL;
    case PTYPE_INSTANCE_DEC:
      return expression == PTYPE_OBJECT || expression == PTYPE_INSTANCE;
    case PTYPE_INSTANCE_PLANE_DEC:
      return expression == PTYPE_OBJECT || expression == PTYPE_INSTANCE_PLANE;
    case PTYPE_INT_DEC:
      return expression == PTYPE_INT;
    case PTYPE_FLOAT_DEC:
      return expression == PTYPE_FLOAT;
    case PTYPE_DICT_DEC:
      return expression == PTYPE_OBJECT;
    case PTYPE_STRING_DEC:
      return expression == PTYPE_STRING;
    case PTYPE_GENERIC_DEC:
      return true;
    case PTYPE_LEVEL_DEC:
      return expression == PTYPE_OBJECT || expression == PTYPE_LEVEL;
    default:
      return false;
  }
}

/* 
 * function: typecheck_array_assignment
 * -------------------------------------------------- 
 */
bool typecheck_array_assignment(p_context_s *context, tnode_s *typenode, tnode_s *expression) {
  bool result = expression->type == PTYPE_ARRAY;
  if (!result)
    return false;
  return result && typecheck_basic_assignment(context, typenode->type, expression->type);
}

/* 
 * function:	resolve_array_type
 * -------------------------------------------------- 
 */
p_nodetype_e resolve_array_type(tnode_list_s list) {
  int i;

  if (list.size == 0)
    return PTYPE_ANY;

  p_nodetype_e type = list.list[0]->type; 
  if (type == PTYPE_ANY)
    return PTYPE_ANY;

  for (i = 1; i < list.size; i++) {
    p_nodetype_e type_i = list.list[i]->type;
    switch (type) {
      case PTYPE_INT:
        if (type_i != PTYPE_INT)
          if (type_i == PTYPE_FLOAT)
            return PTYPE_FLOAT;
          else
            return PTYPE_ANY;
        break;
      case PTYPE_FLOAT:
        if (type_i != PTYPE_FLOAT && type_i != PTYPE_INT)
          return PTYPE_ANY;
        break;
      case PTYPE_STRING:
        if (type_i != PTYPE_STRING)
          return PTYPE_ANY;
        break;
      case PTYPE_SHADER:
        if (type_i != PTYPE_SHADER) {
          if (type_i == PTYPE_OBJECT)
            type = PTYPE_OBJECT;
          else
            return PTYPE_ANY;
        }
        break;
      case PTYPE_TEXTURE:
        if (type_i != PTYPE_TEXTURE) {
          if (type_i == PTYPE_OBJECT)
            type = PTYPE_OBJECT;
          else
            return PTYPE_ANY;
        }
        break;
      case PTYPE_PROGRAM:
        if (type_i != PTYPE_PROGRAM) {
          if (type_i == PTYPE_OBJECT)
            type = PTYPE_OBJECT;
          else
            return PTYPE_ANY;
        }
        break;
      case PTYPE_MESH:
        if (type_i != PTYPE_MESH) {
          if (type_i == PTYPE_OBJECT)
            type = PTYPE_OBJECT;
          else
            return PTYPE_ANY;
        }
        break;
      case PTYPE_MODEL:
        if (type_i != PTYPE_MODEL) {
          if (type_i == PTYPE_OBJECT)
            type = PTYPE_OBJECT;
          else
            return PTYPE_ANY;
        }
        break;
      case PTYPE_INSTANCE:
        if (type_i != PTYPE_INSTANCE) {
          if (type_i == PTYPE_OBJECT)
            type = PTYPE_OBJECT;
          else
            return PTYPE_ANY;
        }
      case PTYPE_INSTANCE_PLANE:
        if (type_i != PTYPE_INSTANCE_PLANE) {
          if (type_i == PTYPE_OBJECT)
            type = PTYPE_OBJECT;
          else
            return PTYPE_ANY;
        }
      case PTYPE_OBJECT:
        switch (type_i) {
          case PTYPE_SHADER:
          case PTYPE_TEXTURE:
          case PTYPE_PROGRAM:
          case PTYPE_MESH:
          case PTYPE_MODEL:
          case PTYPE_INSTANCE:
          case PTYPE_INSTANCE_PLANE:
          case PTYPE_OBJECT:
          case PTYPE_LEVEL:
            break;
          default:
            return PTYPE_ANY;
        }
        break;
      case PTYPE_ARRAY:
        if (type_i != PTYPE_ARRAY)
          return PTYPE_ANY;
        break;
      case PTYPE_LEVEL:
        if (type_i != PTYPE_LEVEL) {
          if (type_i == PTYPE_OBJECT)
            type = PTYPE_OBJECT;
          else
            return PTYPE_ANY;
        }
        break;
      default:
        return PTYPE_ANY;
    }
  }
  return type;
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
    case PTYPE_INSTANCE:
    case PTYPE_INSTANCE_PLANE:
    case PTYPE_OBJECT:
    case PTYPE_LEVEL:
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
    case PTYPE_INSTANCE:
    case PTYPE_INSTANCE_PLANE:
    case PTYPE_OBJECT:
    case PTYPE_LEVEL:
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
symtable_node_s *sym_lookup(p_context_s *context, char *key) {
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
tnode_s *tnode_create_str(tok_s *tok, char *s) {
  tnode_s *t = malloc(sizeof *t);	
  if (!t) {
    return NULL;
  }
  t->type = PTYPE_STRING;
  t->tok = tok;
  t->val.s = s;
  return t;
}

/* 
 * function:	tnode_create_int
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_int(tok_s *tok, int i) {
  tnode_s *t = malloc(sizeof *t);	
  if (!t) {
    return NULL;
  }
  t->type = PTYPE_INT;
  t->tok = tok;
  t->val.i = i;
  return t;
}

/* 
 * function:	tnode_create_float
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_float(tok_s *tok, double f) {
  tnode_s *t = malloc(sizeof *t);	
  if (!t) {
    return NULL;
  }
  t->type = PTYPE_FLOAT;
  t->tok = tok;
  t->val.f = f;
  return t;
}

/* 
 * function:	tnode_create_object
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_object(tok_s *tok, StrMap *obj) {
  tnode_s *t = malloc(sizeof *t);	
  if (!t) {
    return NULL;
  }
  t->type = PTYPE_OBJECT;
  t->tok = tok;
  t->val.obj = obj;
  return t;
}

/* 
 * function:	tnode_create_array
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_array(tok_s *tok, tnode_list_s list, p_nodetype_e type) {
  tnode_s *t = malloc(sizeof *t);	
  if (!t) {
    return NULL;
  }
  t->type = PTYPE_ARRAY;
  t->tok = tok;
  t->val.atval.type = type;
  t->val.atval.arr = list;
  return t;
}

/* 
 * function:	tnode_create_basic_type
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_basic_type(tok_s *tok, p_nodetype_e type) {
  tnode_s *t = malloc(sizeof *t);	
  if (!t) {
    return NULL;
  }
  t->type = type;
  t->tok = tok;
  return t;
}

/* 
 * function:	tnode_create_array_type
 * -------------------------------------------------- 
 */
tnode_s *tnode_create_array_type(tok_s *tok, tnode_arraytype_val_s atval) {
  tnode_s *t = malloc(sizeof *t);
  t->type = PTYPE_ARRAY_DEC;
  t->tok = tok;
  t->val.atval = atval;
  return t;
}

/* 
 * function:	make_label
 * -------------------------------------------------- 
 */
char *make_label(p_context_s *context, char *prefix) {
  size_t len = strlen(prefix) + 2;
  char *buffer= malloc(len);
  if (!buffer) {
    perror("memory allocation error in make_label()");
    return NULL;
  }
  context->labelcount++;
  size_t reallen = snprintf(buffer, len, "%s%u", prefix, context->labelcount);
  if (reallen + 1 > len) {
    buffer = realloc(buffer, reallen + 1);
    sprintf(buffer, "%s%u", prefix, context->labelcount);
  }
  return buffer;
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
  list->list[list->size - 1] = node;
  return 0;
}

void emit_code(const char *code, CharBuf *segment) {
  char_add_s(segment, code);
}

void emit_instance_batch(p_context_s *context, char *levelid, tnode_list_s instances) {
  int i; 
  const bool *isgen;

  emit_code(" INSERT INTO instance(modelID, levelID, vx, vy, vz, scalex, scaley, scalez, mass, isSubjectToGravity, isStatic) VALUES\n", &context->instancecode);
  for (i = 0; i < instances.size - 1; i++) {
    emit_code(" \t", &context->instancecode);
    emit_instance(context, levelid, instances.list[i]);
    emit_code(",\n", &context->instancecode);
  }
  emit_code(" \t", &context->instancecode);
  emit_instance(context, levelid, instances.list[i]);
  emit_code(";\n", &context->instancecode);
}

void emit_instance_plane_batch(p_context_s *context, char *levelid, tnode_list_s instances) {
  int i;
  const bool *isgen;

  emit_code(" INSERT INTO instance_plane(modelID, levelID, v1x, v1y, v1z, v1n, v2x, v2y, v2z, v2n) VALUES\n",
      &context->instanceplanecode);
  for (i = 0; i < instances.size - 1; i++) {
    emit_code(" \t", &context->instanceplanecode); 
    emit_instance_plane(context, levelid, instances.list[i]);
    emit_code(",\n", &context->instanceplanecode); 
  }
  emit_code(" \t", &context->instanceplanecode); 
  emit_instance_plane(context, levelid, instances.list[i]);
  emit_code(";\n", &context->instanceplanecode); 
}

bool emit_instance(p_context_s *context, char *levelid, tnode_s *instance) {
  bool *isgen;
  StrMap *obj = instance->val.obj;
  tnode_s *model = bob_str_map_get(obj, M_KEY("model"));
  if (!model) {
    report_semantics_error("Instance missing required 'model' property", context);
    return false;
  }
  tnode_s *vx = bob_str_map_get(obj, M_KEY("x"));

  if (!vx) {
    report_semantics_error("Instance missing required 'x' property", context);
    return false;
  }
  CharBuf xbuf = val_to_str(vx);

  tnode_s *vy = bob_str_map_get(obj, M_KEY("y"));
  if (!vy) {
    report_semantics_error("Instance missing required 'y' property", context);
    return false;
  }
  CharBuf ybuf = val_to_str(vy);

  tnode_s *vz = bob_str_map_get(obj, M_KEY("z"));
  if (!vz) {
    report_semantics_error("Instance missing required 'z' property", context);
    fprintf(stderr, "lineno: %d\n", instance->tok->lineno);
    return false;
  }
  CharBuf zbuf = val_to_str(vz);

  tnode_s *scalex = bob_str_map_get(obj, M_KEY("scalex"));
  CharBuf scalexbuf;
  if (scalex) {
    scalexbuf = val_to_str(scalex);
  }
  else {
    char_buf_init(&scalexbuf);
    char_add_s(&scalexbuf, "1.0");
  }

  tnode_s *scaley = bob_str_map_get(obj, M_KEY("scaley"));
  CharBuf scaleybuf;
  if (scaley) {
    scaleybuf = val_to_str(scaley);
  }
  else {
    char_buf_init(&scaleybuf);
    char_add_s(&scaleybuf, "1.0");
  }

  tnode_s *scalez= bob_str_map_get(obj, M_KEY("scalez"));
  CharBuf scalezbuf;
  if (scalez) {
    scalezbuf = val_to_str(scalez);
  }
  else {
    char_buf_init(&scalezbuf);
    char_add_s(&scalezbuf, "1.0");
  }

  tnode_s *mass = bob_str_map_get(obj, M_KEY("mass"));
  if (!mass) {
    report_semantics_error("Instance missing required 'mass' property", context);
    return false;
  }
  CharBuf massbuf = val_to_str(mass);

  tnode_s *isSubjectToGravity = bob_str_map_get(obj, M_KEY("isSubjectToGravity"));
  CharBuf isSubjectToGravitybuf;
  if (isSubjectToGravity) {
    isSubjectToGravitybuf = val_to_str(isSubjectToGravity);
  } else {
    char_buf_init(&isSubjectToGravitybuf);
    char_add_s(&isSubjectToGravitybuf, "0");
  }

  tnode_s *isStatic = bob_str_map_get(obj, M_KEY("isStatic"));
  CharBuf isStaticbuf;
  if (isStatic) {
    isStaticbuf = val_to_str(isStatic);
  } else {
    char_buf_init(&isStaticbuf);
    char_add_s(&isStaticbuf, "1");
  }

  char *model_name;
  isgen = bob_str_map_get(model->val.obj, OBJ_ISGEN_KEY);
  if (!*isgen) {
    emit_model(model, context);
  }
  model_name = bob_str_map_get(model->val.obj, OBJ_ID_KEY);
  if (!model_name) {
    report_semantics_error("Internal compiler error, autogenerated name not found in object", context); 
    return false;
  }

  emit_code("((SELECT id FROM ", &context->instancecode);
  emit_code(model_name, &context->instancecode);
  emit_code("),(SELECT id FROM ", &context->instancecode);
  emit_code(levelid, &context->instancecode);
  emit_code("),", &context->instancecode);
  emit_code(xbuf.buffer, &context->instancecode);
  emit_code(",", &context->instancecode);
  emit_code(ybuf.buffer, &context->instancecode);
  emit_code(",", &context->instancecode);
  emit_code(zbuf.buffer, &context->instancecode);
  emit_code(",", &context->instancecode);
  emit_code(scalexbuf.buffer, &context->instancecode);
  emit_code(",", &context->instancecode);
  emit_code(scaleybuf.buffer, &context->instancecode);
  emit_code(",", &context->instancecode);
  emit_code(scalezbuf.buffer, &context->instancecode);
  emit_code(",", &context->instancecode);
  emit_code(massbuf.buffer, &context->instancecode);
  emit_code(",", &context->instancecode);
  emit_code(isSubjectToGravitybuf.buffer, &context->instancecode);
  emit_code(",", &context->instancecode);
  emit_code(isStaticbuf.buffer, &context->instancecode);
  emit_code(")", &context->instancecode);
  char_buf_free(&xbuf);
  char_buf_free(&ybuf);
  char_buf_free(&zbuf);
  char_buf_free(&scalexbuf);
  char_buf_free(&scaleybuf);
  char_buf_free(&scalezbuf);
  char_buf_free(&massbuf);
  char_buf_free(&isSubjectToGravitybuf);
  char_buf_free(&isStaticbuf);
}

bool emit_level(p_context_s *context, tnode_s *level) {
  bool result;
  tnode_s *name_node = bob_str_map_get(level->val.obj, M_KEY("name"));
  if (!name_node) {
    report_semantics_error("Level missing required 'name' property", context);
    return false;
  }
  char *raw_name = name_node->val.s;
  char *table_name = strip_quotes(raw_name);

  tnode_s *n_agx, *n_agy, *n_agz;
  tnode_s *ambient_gravity_node = bob_str_map_get(level->val.obj, M_KEY("ambientGravity"));
  if (ambient_gravity_node) {
    if (ambient_gravity_node->type == PTYPE_ARRAY) {
      tnode_list_s ambient_gravity_array = ambient_gravity_node->val.atval.arr;
      if (ambient_gravity_array.size == 3) {
        tnode_s **glist = ambient_gravity_array.list;
        n_agx = glist[0];
        if (n_agx->type != PTYPE_INT && n_agx->type != PTYPE_FLOAT) {
          report_semantics_error("Ambient Gravity x-component is not a numeric type", context);
          return false;
        }
        n_agy = glist[1];
        if (n_agy->type != PTYPE_INT && n_agy->type != PTYPE_FLOAT) {
          report_semantics_error("Ambient Gravity y-component is not a numeric type", context);
          return false;
        }
        n_agz = glist[2];
        if (n_agz->type != PTYPE_INT && n_agz->type != PTYPE_FLOAT) {
          report_semantics_error("Ambient Gravity z-component is not a numeric type", context);
          return false;
        }
      } 
      else {
        report_semantics_error("ambientGravity must be an array with 3 elements", context);
        return false;
      }
    } 
    else {
      report_semantics_error("ambientGravity must be of type array", context);
      return false;
    }
  } else {
    n_agx = NULL;
    n_agy = NULL;
    n_agz = NULL;
  }


  emit_code("--------------------------------------------------------------------------------\n", &context->levelcode);
  emit_code("-- GENERATING LEVEL: ", &context->levelcode);
  emit_code(raw_name, &context->levelcode);
  emit_code("\n", &context->levelcode);
  emit_code("--------------------------------------------------------------------------------\n", &context->levelcode);
  emit_code(" INSERT INTO level(name,ambientGravityX,ambientGravityY,ambientGravityZ) VALUES(", &context->levelcode);
  emit_code(raw_name, &context->levelcode);
  emit_code(",", &context->levelcode);
  if (n_agx == NULL) {
    emit_code("0,0,0", &context->levelcode);
  }
  else {
    CharBuf agx = val_to_str(n_agx);
    CharBuf agy = val_to_str(n_agy);
    CharBuf agz = val_to_str(n_agz);
    emit_code(agx.buffer, &context->levelcode);
    emit_code(",", &context->levelcode);
    emit_code(agy.buffer, &context->levelcode);
    emit_code(",", &context->levelcode);
    emit_code(agz.buffer, &context->levelcode);
    char_buf_free(&agx);
    char_buf_free(&agy);
    char_buf_free(&agz);
  }
  emit_code(");\n", &context->levelcode);
  emit_code(" CREATE TEMP TABLE ", &context->levelcode);
  emit_code(table_name, &context->levelcode);
  emit_code("(id INTEGER PRIMARY KEY);\n", &context->levelcode);
  emit_code(" INSERT INTO ", &context->levelcode);
  emit_code(table_name, &context->levelcode);
  emit_code("(id) VALUES (last_insert_rowid());\n", &context->levelcode);
  emit_code("----------------------------------------------------------------------------------\n", &context->levelcode); 
  result = emit_instance_data(context, level, table_name);
  free(table_name);
  return result;
}

bool emit_model(tnode_s *model, p_context_s *context) {
  const bool *isgen;
  char *name = bob_str_map_get(model->val.obj, OBJ_ID_KEY);
  if (!name) {
    report_semantics_error("Internal compiler error, autogenerated name not found in object", context); 
    return false;
  }

  char *mesh_name;
  tnode_s *mesh = bob_str_map_get(model->val.obj, M_KEY("mesh"));
  if (!mesh) {
    report_semantics_error("Missing: Expected 'mesh' property in model object.", context);
    return false;
  }
  isgen = bob_str_map_get(mesh->val.obj, OBJ_ISGEN_KEY);
  if (!*isgen) {
    emit_mesh(mesh, context);
  }
  mesh_name = bob_str_map_get(mesh->val.obj, OBJ_ID_KEY);
  if (!mesh_name) {
    report_semantics_error("Internal compiler error, autogenerated name not found in object", context); 
    return false;
  }

  char *program_name;
  tnode_s *program = bob_str_map_get(model->val.obj, M_KEY("program"));
  if (!program) {
    report_semantics_error("Missing: Expected 'program' property in model object.", context);
    return false;
  }
  isgen = bob_str_map_get(program->val.obj, OBJ_ISGEN_KEY);
  if (!*isgen) {
    emit_program(program, context);
  }
  program_name = bob_str_map_get(program->val.obj, OBJ_ID_KEY);
  if (!program_name) {
    report_semantics_error("Internal compiler error, autogenerated name not found in object", context); 
    return false;
  }

  char *texture_name;
  tnode_s *texture = bob_str_map_get(model->val.obj, M_KEY("texture"));
  if (!texture) {
    report_semantics_error("Missing: Expected 'texture' property in model object.", context);
    return false;
  }
  isgen = bob_str_map_get(texture->val.obj, OBJ_ISGEN_KEY);
  if (!*isgen) {
    emit_texture(texture, context);
  }
  texture_name = bob_str_map_get(texture->val.obj, OBJ_ID_KEY);
  if (!texture_name) {
    report_semantics_error("Internal compiler error, autogenerated name not found in object", context); 
    return false;
  }

  int hasUV;
  tnode_s *hasUVNode = bob_str_map_get(model->val.obj, M_KEY("hasUV"));
  if (hasUVNode) {
    hasUV = !!hasUVNode->val.i;
  }
  else {
    hasUV = 0;
  }
  CharBuf hasUVStr;
  char_buf_init(&hasUVStr);
  char_add_i(&hasUVStr, hasUV);

  emit_code("--------------------------------------------------------------------------------\n", &context->modelcode);
  emit_code("-- GENERATING MODEL: ", &context->modelcode);
  emit_code(name, &context->modelcode);
  emit_code("\n", &context->modelcode);
  emit_code("--------------------------------------------------------------------------------\n", &context->modelcode);
  emit_code(" INSERT INTO model(meshID,programID,textureID,hasUV) VALUES(\n", &context->modelcode);
  emit_code(" \t(SELECT id FROM ", &context->modelcode);
  emit_code(mesh_name, &context->modelcode);
  emit_code("),\n", &context->modelcode); 
  emit_code(" \t(SELECT id FROM ", &context->modelcode);
  emit_code(program_name, &context->modelcode);
  emit_code("),\n", &context->modelcode); 
  emit_code(" \t(SELECT id FROM ", &context->modelcode);
  emit_code(texture_name, &context->modelcode);
  emit_code("),\n", &context->modelcode); 
  emit_code(" \t", &context->modelcode); 
  emit_code(hasUVStr.buffer, &context->modelcode);
  emit_code(");\n", &context->modelcode);
  emit_code(" CREATE TEMP TABLE ", &context->modelcode);
  emit_code(name, &context->modelcode);
  emit_code("(id INTEGER PRIMARY KEY);\n", &context->modelcode);
  emit_code(" INSERT INTO ", &context->modelcode);
  emit_code(name, &context->modelcode);
  emit_code("(id) VALUES (last_insert_rowid());\n", &context->modelcode);
  bob_str_map_update(model->val.obj, OBJ_ISGEN_KEY, (void *)&isgen_true);

  char_buf_free(&hasUVStr);
  return true;
}

bool emit_mesh(tnode_s *mesh, p_context_s *context) {
  int i;
  const bool *isgen;
  char *name = bob_str_map_get(mesh->val.obj, OBJ_ID_KEY);
  if (!name) {
    report_semantics_error("Internal compiler error, autogenerated name not found in object", context); 
    return false;
  }
  char *sname;
  tnode_s *sname_node = bob_str_map_get(mesh->val.obj, M_KEY("name"));
  if (sname_node) {
    sname = sname_node->val.s;
  }
  else {
    sname = name;
  }
  tnode_s *vertices = bob_str_map_get(mesh->val.obj, M_KEY("vertices"));
  if (!vertices) {
    report_semantics_error("Missing: Expected 'vertices' property in mesh object.", context);
    return false;
  }
  if (vertices->type != PTYPE_ARRAY) {
    report_semantics_error("Expected array type for vertices", context);
    return false;
  }
  if (vertices->val.atval.type != PTYPE_INT && vertices->val.atval.type != PTYPE_FLOAT) {
    report_semantics_error("Expected array of type integer or float for vertices", context);
    return false;
  }
  tnode_list_s vertex_array = vertices->val.atval.arr;

  tnode_s *tnode;
  CharBuf vertexstr;
  CharBuf numbuf;
  char_buf_init(&vertexstr);

  for (i = 0; i < vertex_array.size - 1; i++) {
    tnode = vertex_array.list[i];
    if (tnode->type == PTYPE_INT) {
      char_buf_init(&numbuf);
      char_add_i(&numbuf, tnode->val.i);
      char_add_s(&vertexstr, numbuf.buffer);
      char_buf_free(&numbuf);
    }
    else if (tnode->type == PTYPE_FLOAT) {
      char_buf_init(&numbuf);
      char_add_d(&numbuf, tnode->val.f);
      char_add_s(&vertexstr, numbuf.buffer);
      char_buf_free(&numbuf);
    }
    else
      fprintf(stderr, "Unknown type %d for mesh array.\n", tnode->type);
    char_add_s(&vertexstr, ",");
  }
  tnode = vertex_array.list[i];
  if (tnode->type == PTYPE_INT) {
      char_buf_init(&numbuf);
      char_add_i(&numbuf, tnode->val.i);
      char_add_s(&vertexstr, numbuf.buffer);
      char_buf_free(&numbuf);
  }
  else if (tnode->type == PTYPE_FLOAT) {
      char_buf_init(&numbuf);
      char_add_d(&numbuf, tnode->val.f);
      char_add_s(&vertexstr, numbuf.buffer);
      char_buf_free(&numbuf);
  }
  else
    fprintf(stderr, "Unknown type %d for mesh array.\n", tnode->type);

  emit_code("--------------------------------------------------------------------------------\n", &context->meshcode);
  emit_code("-- GENERATING MESH: ", &context->meshcode);
  emit_code(name, &context->meshcode);
  emit_code("\n", &context->meshcode);
  emit_code("--------------------------------------------------------------------------------\n", &context->meshcode);
  emit_code(" INSERT INTO mesh(name,data) VALUES(", &context->meshcode);
  emit_code("\"", &context->meshcode);
  emit_code(sname, &context->meshcode);
  emit_code("\",\"", &context->meshcode);
  emit_code(vertexstr.buffer, &context->meshcode);
  emit_code("\");\n", &context->meshcode);
  emit_code(" CREATE TEMP TABLE ", &context->meshcode);
  emit_code(name, &context->meshcode);
  emit_code("(id INTEGER PRIMARY KEY);\n", &context->meshcode);
  emit_code(" INSERT INTO ", &context->meshcode);
  emit_code(name, &context->meshcode);
  emit_code("(id) VALUES (last_insert_rowid());\n", &context->meshcode);

  char_buf_free(&vertexstr);
  bob_str_map_update(mesh->val.obj, OBJ_ISGEN_KEY, (void *)&isgen_true);
  return true;
}

bool emit_program(tnode_s *program, p_context_s *context) {
  const bool *isgen;

  char *name = bob_str_map_get(program->val.obj, OBJ_ID_KEY);
  if (!name) {
    report_semantics_error("Internal compiler error, autogenerated name not found in object", context); 
    return false;
  }
  emit_code("--------------------------------------------------------------------------------\n", &context->programcode);
  emit_code("-- GENERATING PROGRAM: ", &context->programcode);
  emit_code(name, &context->programcode);
  emit_code("\n", &context->programcode);
  emit_code("--------------------------------------------------------------------------------\n", &context->programcode);
  emit_code(" INSERT INTO program DEFAULT VALUES;\n", &context->programcode);

  emit_code(" CREATE TEMP TABLE ", &context->programcode);
  emit_code(name, &context->programcode);
  emit_code("(id INTEGER PRIMARY KEY);\n", &context->programcode);
  emit_code(" INSERT INTO ", &context->programcode);
  emit_code(name, &context->programcode);
  emit_code("(id) VALUES (last_insert_rowid());\n", &context->programcode);

  check_shader(program, context, M_KEY("vertex"), name, BOB_VERTEX_SHADER);
  check_shader(program, context, M_KEY("tess_eval"), name, BOB_TESS_EVAL_SHADER);
  check_shader(program, context, M_KEY("tess_control"), name, BOB_TESS_CONTROL_SHADER);
  check_shader(program, context, M_KEY("geometry"), name, BOB_GEOMETRY_SHADER);
  check_shader(program, context, M_KEY("fragment"), name, BOB_FRAGMENT_SHADER);
  check_shader(program, context, M_KEY("compute"), name, BOB_COMPUTE_SHADER);

  bob_str_map_update(program->val.obj, OBJ_ISGEN_KEY, (void *)&isgen_true);
  return true;
}

bool emit_shader(tnode_s *shader, bob_shader_e type, p_context_s *context) {
  char *name = bob_str_map_get(shader->val.obj, OBJ_ID_KEY);
  if (!name) {
    report_semantics_error("Internal compiler error, autogenerated name not found in object", context); 
    return false;
  }

  char *sname;
  tnode_s *sname_node = bob_str_map_get(shader->val.obj, M_KEY("name"));
  if (!sname_node) {
    sname = name;
  }
  else {
    sname = sname_node->val.s;
  }

  char *src, *src_stripped;
  tnode_s *src_node = bob_str_map_get(shader->val.obj, M_KEY("src"));
  if (!src) {
    report_semantics_error("Shader missing required 'src' property", context); 
    return false;
  }
  src = src_node->val.s;
  src_stripped = strip_quotes(src);

  CharBuf typebuf, srcbuf;
  char_buf_init(&typebuf);
  char_add_i(&typebuf, type);
  

  srcbuf = pad_quotes(src_stripped);

  emit_code("--------------------------------------------------------------------------------\n", &context->shadercode);
  emit_code("-- GENERATING SHADER: ", &context->shadercode);
  emit_code(name, &context->shadercode);
  emit_code("\n", &context->shadercode);
  emit_code("--------------------------------------------------------------------------------\n", &context->shadercode);
  emit_code(" INSERT INTO shader(type,name,src) VALUES(", &context->shadercode);
  emit_code(typebuf.buffer, &context->shadercode);
  emit_code(",", &context->shadercode);
  emit_code("\"", &context->shadercode);
  emit_code(sname, &context->shadercode);
  emit_code("\",\"", &context->shadercode);
  emit_code(srcbuf.buffer, &context->shadercode);
  emit_code("\");\n", &context->shadercode);
  emit_code(" CREATE TEMP TABLE ", &context->shadercode);
  emit_code(name, &context->shadercode);
  emit_code("(id INTEGER PRIMARY KEY);\n", &context->shadercode);
  emit_code(" INSERT INTO ", &context->shadercode);
  emit_code(name, &context->shadercode);
  emit_code("(id) VALUES (last_insert_rowid());\n", &context->shadercode);
  bob_str_map_update(shader->val.obj, OBJ_ISGEN_KEY, (void *)&isgen_true);
  char_buf_free(&typebuf);
  char_buf_free(&srcbuf);
  return true;
}

bool emit_texture(tnode_s *texture, p_context_s *context) {
  char *name = bob_str_map_get(texture->val.obj, OBJ_ID_KEY);
  if (!name) {
    report_semantics_error("Internal compiler error, autogenerated name not found in object", context); 
    return false;
  }
  char *path;
  tnode_s *path_node = bob_str_map_get(texture->val.obj, M_KEY("path"));
  if (!path_node) {
    report_semantics_error("Error in texture: expected 'path' property", context); 
    return false;
  }
  path = path_node->val.s;

  emit_code("--------------------------------------------------------------------------------\n", &context->texturecode);
  emit_code("-- GENERATING TEXTURE: ", &context->texturecode);
  emit_code(name, &context->texturecode);
  emit_code("\n", &context->texturecode);
  emit_code("--------------------------------------------------------------------------------\n", &context->texturecode);
  emit_code(" INSERT INTO texture(path) VALUES(", &context->texturecode);
  emit_code(path, &context->texturecode);
  emit_code(");\n", &context->texturecode);
  emit_code(" CREATE TEMP TABLE ", &context->texturecode);
  emit_code(name, &context->texturecode);
  emit_code("(id INTEGER PRIMARY KEY);\n", &context->texturecode);
  emit_code(" INSERT INTO ", &context->texturecode);
  emit_code(name, &context->texturecode);
  emit_code("(id) VALUES (last_insert_rowid());\n", &context->texturecode);
  bob_str_map_update(texture->val.obj, OBJ_ISGEN_KEY, (void *)&isgen_true);
  return true;
}

bool emit_instance_data(p_context_s *context, tnode_s *level, char *levelid) {
  tnode_s *instances = bob_str_map_get(level->val.obj, M_KEY("instances"));
  if (instances) {
    emit_instances(context, levelid, instances);
  }
  tnode_s *instance_planes = bob_str_map_get(level->val.obj, M_KEY("instancePlanes"));
  if (instance_planes) {
    emit_instance_planes(context, levelid, instance_planes);
  }
  if (!instances && !instance_planes) {
    report_semantics_error("Level must at least have an instances or instancePlanes array", context);
    return false;
  }
}

bool emit_instances(p_context_s *context, char *levelid, tnode_s *instances) {
  //TODO: check if array type
  tnode_arraytype_val_s atval = instances->val.atval;
  if (atval.type == PTYPE_OBJECT || atval.type == PTYPE_INSTANCE) {
    emit_instance_batch(context, levelid, atval.arr);
  }
  else {
    report_semantics_error("Instances must be an array of objects or instance types", context);
    return false;
  }
  return true;
}

bool emit_instance_planes(p_context_s *context, char *levelid, tnode_s *instance_planes) {
  tnode_arraytype_val_s atval = instance_planes->val.atval;
  if (atval.type == PTYPE_OBJECT || atval.type == PTYPE_INSTANCE_PLANE) {
    emit_instance_plane_batch(context, levelid, atval.arr);
  }
  else {
    report_semantics_error("instancePlanes must be an array of objects or instance plane types", context);
    return false;
  }
  return true;
}

bool emit_instance_plane(p_context_s *context, char *levelid, tnode_s *instance_plane) {
  bool *isgen;
  StrMap *obj = instance_plane->val.obj;
  tnode_s *model = bob_str_map_get(obj, M_KEY("model"));
  if (!model) {
    report_semantics_error("Instance Plane missing required 'model' property", context);
    return false;
  }

  tnode_s *vx1 = bob_str_map_get(obj, M_KEY("x1"));
  if (!vx1) {
    report_semantics_error("Instance missing required 'x1' property", context);
    return false;
  }
  CharBuf x1buf = val_to_str(vx1);

  tnode_s *vy1 = bob_str_map_get(obj, M_KEY("y1"));
  if (!vy1) {
    report_semantics_error("Instance missing required 'y1' property", context);
    return false;
  }
  CharBuf y1buf = val_to_str(vy1);

  tnode_s *vz1 = bob_str_map_get(obj, M_KEY("z1"));
  if (!vz1) {
    report_semantics_error("Instance missing required 'z1' property", context);
    return false;
  }
  CharBuf z1buf = val_to_str(vz1);

  tnode_s *vn1 = bob_str_map_get(obj, M_KEY("n1"));
  if (!vn1) {
    report_semantics_error("Instance missing required 'n1' property", context);
    return false;
  }
  CharBuf n1buf = val_to_str(vn1);

  tnode_s *vx2 = bob_str_map_get(obj, M_KEY("x2"));
  if (!vx2) {
    report_semantics_error("Instance missing required 'x2' property", context);
    return false;
  }
  CharBuf x2buf = val_to_str(vx2);

  tnode_s *vy2 = bob_str_map_get(obj, M_KEY("y2"));
  if (!vy2) {
    report_semantics_error("Instance missing required 'y2' property", context);
    return false;
  }
  CharBuf y2buf = val_to_str(vy2);

  tnode_s *vz2 = bob_str_map_get(obj, M_KEY("z2"));
  if (!vz2) {
    report_semantics_error("Instance missing required 'z2' property", context);
    return false;
  }
  CharBuf z2buf = val_to_str(vz2);

  tnode_s *vn2 = bob_str_map_get(obj, M_KEY("n2"));
  if (!vn2) {
    report_semantics_error("Instance missing required 'n2' property", context);
    return false;
  }
  CharBuf n2buf = val_to_str(vn2);

  char *model_name;
  isgen = bob_str_map_get(model->val.obj, OBJ_ISGEN_KEY);
  if (!*isgen) {
    emit_model(model, context);
  }
  model_name = bob_str_map_get(model->val.obj, OBJ_ID_KEY);
  if (!model_name) {
    report_semantics_error("Internal compiler error, autogenerated name not found in object", context); 
    return false;
  }

  emit_code("((SELECT id FROM ", &context->instanceplanecode);
  emit_code(model_name, &context->instanceplanecode);
  emit_code("),(SELECT id FROM ", &context->instanceplanecode);
  emit_code(levelid, &context->instanceplanecode);
  emit_code("),", &context->instanceplanecode);
  emit_code(x1buf.buffer, &context->instanceplanecode);
  emit_code(",", &context->instanceplanecode);
  emit_code(y1buf.buffer, &context->instanceplanecode);
  emit_code(",", &context->instanceplanecode);
  emit_code(z1buf.buffer, &context->instanceplanecode);
  emit_code(",", &context->instanceplanecode);
  emit_code(n1buf.buffer, &context->instanceplanecode);
  emit_code(",", &context->instanceplanecode);
  emit_code(x2buf.buffer, &context->instanceplanecode);
  emit_code(",", &context->instanceplanecode);
  emit_code(y2buf.buffer, &context->instanceplanecode);
  emit_code(",", &context->instanceplanecode);
  emit_code(z2buf.buffer, &context->instanceplanecode);
  emit_code(",", &context->instanceplanecode);
  emit_code(n2buf.buffer, &context->instanceplanecode);
  emit_code(")", &context->instanceplanecode);
  char_buf_free(&x1buf);
  char_buf_free(&y1buf);
  char_buf_free(&z1buf);
  char_buf_free(&n1buf);
  char_buf_free(&x2buf);
  char_buf_free(&y2buf);
  char_buf_free(&z2buf);
  char_buf_free(&n2buf);
  return true;
}

bool check_shader(tnode_s *program, p_context_s *context, const char *shader_key, const char *programname, bob_shader_e type) {
  bool *isgen;
  tnode_s *shader = bob_str_map_get(program->val.obj, shader_key);
  if (shader) {
    isgen = bob_str_map_get(program->val.obj, OBJ_ISGEN_KEY);
    if (!*isgen) {
      emit_shader(shader, type, context);
    }
    char *shadername = bob_str_map_get(shader->val.obj, OBJ_ID_KEY);
    if (!shadername) {
      report_semantics_error("Internal compiler error, autogenerated name not found in object", context); 
      return false;
    }
    emit_code(" INSERT INTO program_xref(shaderID, programID) VALUES(\n", &context->programcode);
    emit_code(" \t(SELECT id FROM ", &context->programcode);
    emit_code(shadername, &context->programcode);
    emit_code("),\n \t(SELECT id FROM ", &context->programcode);
    emit_code(programname, &context->programcode);
    emit_code(")\n );\n", &context->programcode);
  }
}

CharBuf val_to_str(tnode_s *val) {
  CharBuf buf;

  char_buf_init(&buf);
  if (val->type == PTYPE_INT) {
    char_add_i(&buf, val->val.i);
  }
  else if (val->type == PTYPE_FLOAT) {
    char_add_d(&buf, val->val.f);
  }
  else if (val->type == PTYPE_STRING) {
    char_add_s(&buf, val->val.s);
  }
  else {
    fprintf(stderr, "Unexpected type %d to convert to string.\n", val->type);
  }
  return buf;
}

void gen_code(p_context_s *context, FILE *dest) {
  fprintf(dest, 
    "/*********************************************************************************\n"
    "* MESHES\n"
    "*********************************************************************************/\n");
  fprintf(dest, "%s", context->meshcode.buffer);
  fprintf(dest, "/********************************************************************************/\n");

  fprintf(dest, 
    "/*********************************************************************************\n"
    "* SHADERS\n"
    "*********************************************************************************/\n");
  fprintf(dest, "%s", context->shadercode.buffer);
  fprintf(dest, "/********************************************************************************/\n");

  fprintf(dest, 
    "/*********************************************************************************\n"
    "* PROGRAMS\n"
    "*********************************************************************************/\n");
  fprintf(dest, "%s", context->programcode.buffer);
  fprintf(dest, "/********************************************************************************/\n");

  fprintf(dest, 
    "/*********************************************************************************\n"
    "* TEXTURES\n"
    "*********************************************************************************/\n");
  fprintf(dest, "%s", context->texturecode.buffer);
  fprintf(dest, "/********************************************************************************/\n");

  fprintf(dest, 
    "/*********************************************************************************\n"
    "* MODELS\n"
    "*********************************************************************************/\n");
  fprintf(dest, "%s", context->modelcode.buffer);
  fprintf(dest, "/********************************************************************************/\n");

  fprintf(dest, 
    "/*********************************************************************************\n"
    "* LEVELS\n"
    "*********************************************************************************/\n");
  fprintf(dest, "%s", context->levelcode.buffer);
  fprintf(dest, "/********************************************************************************/\n");

  fprintf(dest, 
    "/*********************************************************************************\n"
    "* INSTANCES\n"
    "*********************************************************************************/\n");
  fprintf(dest, "%s", context->instancecode.buffer);
  fprintf(dest, "/********************************************************************************/\n");

  fprintf(dest, 
    "/*********************************************************************************\n"
    "* INSTANCE PLANES\n"
    "*********************************************************************************/\n");
  fprintf(dest, "%s", context->instanceplanecode.buffer);
  fprintf(dest, "/********************************************************************************/\n");
}

char *strip_quotes(char *level_name) {
  int i;
  size_t nlen = strlen(level_name) - 1;
  char *nname = malloc(nlen);

  if (!nname) {
    perror("Memory allocation error in strip_quotes()");
    return NULL;
  }
  for (i = 0; i < nlen - 1; i++) {
    nname[i] = level_name[i+1];
  }
  nname[i] = '\0';
  return nname;
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


