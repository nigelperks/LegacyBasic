// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <assert.h>
#include <setjmp.h>
#include "parse.h"
#include "lexer.h"
#include "token.h"
#include "utils.h"
#include "emit.h"
#include "bcode.h"
#include "builtin.h"
#include "os.h"

typedef struct {
  LEX* lex;
  BCODE* bcode;
  STRINGLIST* names;
  unsigned if_then;
  jmp_buf errjmp;
} PARSER;

static void parse_line(PARSER*, unsigned line_index, unsigned lineno, const char* text);

BCODE* parse_source(const SOURCE* source, STRINGLIST* names, bool recognise_keyword_prefixes) {
  assert(source != NULL && names != NULL);
  PARSER parser;
  parser.lex = new_lex(source_name(source), recognise_keyword_prefixes);
  parser.bcode = new_bcode();
  parser.names = names;
  parser.if_then = 0;
  if (setjmp(parser.errjmp) == 0) {
    for (unsigned i = 0; i < source_lines(source); i++)
      parse_line(&parser, i, source_linenum(source, i), source_text(source, i));
  }
  else {
    delete_bcode(parser.bcode);
    parser.bcode = NULL;
  }
  delete_lex(parser.lex);
  return parser.bcode;
}

static void print_line(LEX* lex) {
  unsigned lineno = lex_line_num(lex);
  int len = lineno ? fprintf(stderr, "%u ", lineno) : 0;
  fprintf(stderr, "%s\n", lex_line_text(lex));
  space(len + lex_token_pos(lex), stderr);
  fputs("^\n", stderr);
}

static void parse_error_message(LEX* lex, const char* fmt, ...) {
  print_line(lex);

  fputs("Syntax error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputs(": ", stderr);
  print_lex_token(lex, stderr);
  putc('\n', stderr);
}

static void parse_error(PARSER* parser, const char* fmt, ...) {
  print_line(parser->lex);

  fputs("Syntax error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputs(": ", stderr);
  print_lex_token(parser->lex, stderr);
  putc('\n', stderr);

  longjmp(parser->errjmp, 1);
}

static void parse_error_no_token(PARSER* parser, const char* fmt, ...) {
  print_line(parser->lex);

  fputs("Syntax error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  putc('\n', stderr);

  longjmp(parser->errjmp, 1);
}

static void parse_error_token(PARSER* parser, const char* msg, int token) {
  print_line(parser->lex);

  fprintf(stderr, "Syntax error: %s: ", msg);
  print_token(token, stderr);
  putc('\n', stderr);

  longjmp(parser->errjmp, 1);
}

static void match(PARSER* parser, int token) {
  if (lex_token(parser->lex) != token) {
    parse_error_message(parser->lex, "unexpected token");
    fputs("Expected: ", stderr);
    print_token(token, stderr);
    putc('\n', stderr);
    longjmp(parser->errjmp, 1);
  }
  lex_next(parser->lex);
}

static void complete_statement(PARSER*);

static void parse_line(PARSER* parser, unsigned line_index, unsigned lineno, const char* text) {
  lex_line(parser->lex, lineno, text);
  emit_source_line(parser->bcode, B_SOURCE_LINE, line_index);
  parser->if_then = 0;
  complete_statement(parser);
  while (lex_token(parser->lex) == ':') {
    lex_next(parser->lex);
    complete_statement(parser);
  }
  match(parser, '\n');
}

static bool statement(PARSER*);

// As long as a complete statement continues,
// another statement is required before a colon or end of line.
static void complete_statement(PARSER* parser) {
  while (statement(parser))
    ;
}

static void clear_statement(PARSER*);
static void cls_statement(PARSER*);
static void data_statement(PARSER*);
static void def_statement(PARSER*);
static void dim_statement(PARSER*);
static bool else_clause(PARSER*);
static void end_statement(PARSER*);
static void for_statement(PARSER*);
static void goto_statement(PARSER*);
static void gosub_statement(PARSER*);
static bool if_statement(PARSER*);
static void input_statement(PARSER*);
static void let_statement(PARSER*);
static void line_input_statement(PARSER*);
static void next_statement(PARSER*);
static void on_statement(PARSER*);
static void print_statement(PARSER*);
static void randomize_statement(PARSER*);
static void read_statement(PARSER*);
static void rem_statement(PARSER*);
static void restore_statement(PARSER*);
static void return_statement(PARSER*);
static void stop_statement(PARSER*);

static void assignment(PARSER*);

static bool statement(PARSER* parser) {
  bool continues = false;
  switch (lex_token(parser->lex)) {
    case TOK_CLEAR: clear_statement(parser); break;
    case TOK_CLS: cls_statement(parser); break;
    case TOK_DATA: data_statement(parser); break;
    case TOK_DEF: def_statement(parser); break;
    case TOK_DIM: dim_statement(parser); break;
    case TOK_END: end_statement(parser); break;
    case TOK_FOR: for_statement(parser); break;
    case TOK_GOSUB: gosub_statement(parser); break;
    case TOK_GOTO: goto_statement(parser); break;
    case TOK_IF: continues = if_statement(parser); break;
    case TOK_INPUT: input_statement(parser); break;
    case TOK_LET: let_statement(parser); break;
    case TOK_LINE: line_input_statement(parser); break;
    case TOK_NEXT: next_statement(parser); break;
    case TOK_ON: on_statement(parser); break;
    case TOK_PRINT: case '?': print_statement(parser); break;
    case TOK_RANDOMIZE: randomize_statement(parser); break;
    case TOK_READ: read_statement(parser); break;
    case TOK_REM: rem_statement(parser); break;
    case TOK_RESTORE: restore_statement(parser); break;
    case TOK_RETURN: return_statement(parser); break;
    case TOK_STOP: stop_statement(parser); break;
    case TOK_ID: assignment(parser); break;
    default: parse_error(parser, "statement expected"); break;
  }
  if (lex_token(parser->lex) == TOK_ELSE)
    continues = else_clause(parser);
  return continues;
}

static bool eos(LEX* lex) {
  int t;
  return (t = lex_token(lex)) == TOK_EOF || t == '\n' || t == ':' || t == TOK_ELSE;
}

static int expression(PARSER*);

static void numeric_expression(PARSER*);
static void string_expression(PARSER*);

static unsigned line_number(PARSER*);
static int identifier(PARSER*, unsigned *name, unsigned *parameters);

static void clear_statement(PARSER* parser) {
  match(parser, TOK_CLEAR);
  emit(parser->bcode, B_CLEAR);
}

static void cls_statement(PARSER* parser) {
  match(parser, TOK_CLS);
  emit(parser->bcode, B_CLS);
}

static void datum(PARSER*);

static void data_statement(PARSER* parser) {
  if (lex_token(parser->lex) != TOK_DATA)
    match(parser, TOK_DATA);

  emit_str(parser->bcode, B_DATA, lex_next_data(parser->lex));
  while (lex_next(parser->lex) == ',')
    emit_str(parser->bcode, B_DATA, lex_next_data(parser->lex));
}

static void read_item(PARSER*);

static void read_statement(PARSER* parser) {
  match(parser, TOK_READ);

  read_item(parser);
  while (lex_token(parser->lex) == ',') {
    lex_next(parser->lex);
    read_item(parser);
  }
}

static void read_item(PARSER* parser) {
  unsigned namei, dim;
  int type = identifier(parser, &namei, &dim);
  if (type == TYPE_STR)
    emit_param(parser->bcode, B_READ_STR, namei, dim);
  else {
    assert(type == TYPE_NUM);
    emit_param(parser->bcode, B_READ_NUM, namei, dim);
  }
}

static void restore_statement(PARSER* parser) {
  match(parser, TOK_RESTORE);
  if (lex_token(parser->lex) == TOK_NUM)
    emit_basic_line(parser->bcode, B_RESTORE_LINE, line_number(parser));
  else
    emit(parser->bcode, B_RESTORE);
}

static void parameter(PARSER*);

static void def_statement(PARSER* parser) {
  match(parser, TOK_DEF);

  if (lex_token(parser->lex) != TOK_ID)
    parse_error(parser, "User-defined function name expected");

  assert(parser->names != NULL);
  unsigned name = name_entry(parser->names, lex_word(parser->lex));
  int type = string_name(lex_word(parser->lex)) ? TYPE_STR : TYPE_NUM;
  match(parser, TOK_ID);
  emit_param(parser->bcode, B_DEF, name, 1);

  match(parser, '(');
  parameter(parser);
  match(parser, ')');

  match(parser, '=');
  if (type == TYPE_STR)
    string_expression(parser);
  else
    numeric_expression(parser);
  emit(parser->bcode, B_END_DEF);
}

static void parameter(PARSER* parser) {
  unsigned name, params = 0;
  int type = identifier(parser, &name, &params);
  if (type != TYPE_NUM || params != 0)
    parse_error(parser, "only simple numeric parameters are allowed");
  emit_var(parser->bcode, B_PARAM, name);
}

static void dim_array(PARSER*);

static void dim_statement(PARSER* parser) {
  match(parser, TOK_DIM);

  dim_array(parser);
  while (lex_token(parser->lex) == ',') {
    lex_next(parser->lex);
    dim_array(parser);
  }
}

static void dim_array(PARSER* parser) {
  unsigned namei;
  unsigned dimensions = 0;
  int type = identifier(parser, &namei, &dimensions);
  assert(parser->names != NULL);
  const char* name = stringlist_item(parser->names, namei);
  if (dimensions == 0)
    parse_error(parser, "Subscripted dimensions were expected: %s", name);
  if (type == TYPE_STR)
    emit_param(parser->bcode, B_DIM_STR, namei, dimensions);
  else {
    assert(type == TYPE_NUM);
    emit_param(parser->bcode, B_DIM_NUM, namei, dimensions);
  }
}

static void end_statement(PARSER* parser) {
  match(parser, TOK_END);
  emit(parser->bcode, B_END);
}

static unsigned for_variable(PARSER*);

static void for_statement(PARSER* parser) {
  match(parser, TOK_FOR);

  unsigned namei = for_variable(parser);

  match(parser, '=');
  numeric_expression(parser);
  match(parser, TOK_TO);
  numeric_expression(parser);
  if (lex_token(parser->lex) == TOK_STEP) {
    lex_next(parser->lex);
    numeric_expression(parser);
  }
  else
    emit_num(parser->bcode, B_PUSH_NUM, 1);

  emit_var(parser->bcode, B_FOR, namei);
}

static void next_statement(PARSER* parser) {
  match(parser, TOK_NEXT);

  if (lex_token(parser->lex) == TOK_ID) {
    emit_var(parser->bcode, B_NEXT_VAR, for_variable(parser));
    while (lex_token(parser->lex) == ',') {
      lex_next(parser->lex);
      emit_var(parser->bcode, B_NEXT_VAR, for_variable(parser));
    }
  }
  else
    emit(parser->bcode, B_NEXT_IMP);
}

static unsigned for_variable(PARSER* parser) {
  unsigned namei, dim;
  int type = identifier(parser, &namei, &dim);
  assert(parser->names != NULL);
  const char* name = stringlist_item(parser->names, namei);
  if (type != TYPE_NUM)
    parse_error_no_token(parser, "FOR/NEXT variable must be numeric: %s", name);
  if (dim)
    parse_error_no_token(parser, "FOR/NEXT variable cannot be an array element: %s", name);
  return namei;
}

static void gosub_statement(PARSER* parser) {
  match(parser, TOK_GOSUB);
  unsigned line = line_number(parser);
  emit_basic_line(parser->bcode, B_GOSUB, line);
}

static void goto_statement(PARSER* parser) {
  match(parser, TOK_GOTO);
  unsigned line = line_number(parser);
  emit_basic_line(parser->bcode, B_GOTO, line);
}

// Return true if statement continues: IF ... THEN more-statements
// Return false if statement does not continue: IF ... THEN line-number
static bool if_statement(PARSER* parser) {
  match(parser, TOK_IF);
  numeric_expression(parser);
  match(parser, TOK_THEN);
  if (lex_token(parser->lex) == TOK_NUM) {
    unsigned line = line_number(parser);
    emit_basic_line(parser->bcode, B_GOTRUE, line);
    parser->if_then = 0;
    return false;
  }
  parser->if_then = emit(parser->bcode, B_IF_THEN);
  return true;
}

static bool else_clause(PARSER* parser) {
  if (parser->if_then == 0) {
    const BINST* inst = bcode_latest(parser->bcode);
    if (inst && inst->op == B_GOTRUE) {
      // IF ... THEN line-number ELSE ...
      match(parser, TOK_ELSE);
      unsigned line = line_number(parser);
      emit_basic_line(parser->bcode, B_GOTO, line);
      return false;
    }
    parse_error(parser, "unexpected ELSE");
  }

  // IF ... THEN statements ELSE ...
  match(parser, TOK_ELSE);
  patch_opcode(parser->bcode, parser->if_then, B_IF_ELSE);
  emit(parser->bcode, B_ELSE);
  parser->if_then = 0;
  return true;
}

static void input_buffer(PARSER*);
static void input_item(PARSER*);

static void input_statement(PARSER* parser) {
  match(parser, TOK_INPUT);

  input_buffer(parser);

  input_item(parser);
  while (lex_token(parser->lex) == ',') {
    lex_next(parser->lex);
    emit(parser->bcode, B_INPUT_SEP);
    input_item(parser);
  }

  emit(parser->bcode, B_INPUT_END);
}

static void input_buffer(PARSER* parser) {
  char* prompt = NULL;
  if (lex_token(parser->lex) == TOK_STR) {
    prompt = estrdup(lex_word(parser->lex));
    int sep = lex_next(parser->lex);
    if (sep == ';' || sep == ',')
      lex_next(parser->lex);
  }

  emit_str_ptr(parser->bcode, B_INPUT_BUF, prompt);
}

static void input_item(PARSER* parser) {
  unsigned name;
  unsigned dimensions = 0;
  int type = identifier(parser, &name, &dimensions);
  assert(type == TYPE_NUM || type == TYPE_STR);

  emit_param(parser->bcode, type == TYPE_NUM ? B_INPUT_NUM : B_INPUT_STR, name, dimensions);
}

static void line_input_statement(PARSER* parser) {
  match(parser, TOK_LINE);
  match(parser, TOK_INPUT);

  input_buffer(parser);

  unsigned name, dimensions = 0;
  int type = identifier(parser, &name, &dimensions);
  if (type != TYPE_STR)
    parse_error(parser, "string variable expected");

  emit_param(parser->bcode, B_INPUT_LINE, name, dimensions);
}

static void emit_set(PARSER*, int type, unsigned name, unsigned dimensions);

static void assignment(PARSER* parser) {
  unsigned name;
  unsigned dimensions = 0;
  int type = identifier(parser, &name, &dimensions);
  assert(type == TYPE_NUM || type == TYPE_STR);
  match(parser, '=');
  int e = expression(parser);
  assert(e == TYPE_NUM || e == TYPE_STR);
  if (e != type)
    parse_error(parser, "type mismatch in assignment");
  emit_set(parser, type, name, dimensions);
}

static void emit_set(PARSER* parser, int type, unsigned name, unsigned dimensions) {
  if (dimensions)
    emit_param(parser->bcode, type == TYPE_STR ? B_SET_ARRAY_STR : B_SET_ARRAY_NUM, name, dimensions);
  else
    emit_var(parser->bcode, type == TYPE_STR ? B_SET_SIMPLE_STR : B_SET_SIMPLE_NUM, name);
}

static void let_statement(PARSER* parser) {
  match(parser, TOK_LET);

  assignment(parser);
}

static void on_line(PARSER*);

static void on_statement(PARSER* parser) {
  match(parser, TOK_ON);

  numeric_expression(parser);

  unsigned opcode = B_NOP;
  switch (lex_token(parser->lex)) {
    case TOK_GOTO: opcode = B_ON_GOTO; break;
    case TOK_GOSUB: opcode = B_ON_GOSUB; break;
    default: parse_error(parser, "GOTO or GOSUB expected"); break;
  }

  unsigned i = emit_count(parser->bcode, opcode, 0);
  lex_next(parser->lex);
  on_line(parser);
  unsigned count = 1;
  while (lex_token(parser->lex) == ',') {
    lex_next(parser->lex);
    on_line(parser);
    count++;
  }
  patch_count(parser->bcode, i, count);
}

static void on_line(PARSER* parser) {
  emit_basic_line(parser->bcode, B_ON_LINE, line_number(parser));
}

static void print_builtin(PARSER* parser, int opcode);

static void print_statement(PARSER* parser) {
  if (lex_token(parser->lex) == '?')
    lex_next(parser->lex);
  else
    match(parser, TOK_PRINT);

  int sep = 0;

  while (!eos(parser->lex)) {
    if (lex_token(parser->lex) == TOK_ID) {
      if (STRICMP(lex_word(parser->lex), "SPC") == 0) {
        print_builtin(parser, B_PRINT_SPC);
        sep = 0;
        continue;
      }
      if (STRICMP(lex_word(parser->lex), "TAB") == 0) {
        print_builtin(parser, B_PRINT_TAB);
        sep = 0;
        continue;
      }
    }
    if (lex_token(parser->lex) == ';') {
      lex_next(parser->lex);
      sep = ';';
      continue;
    }
    if (lex_token(parser->lex) == ',') {
      lex_next(parser->lex);
      emit(parser->bcode, B_PRINT_COMMA);
      sep = ',';
      continue;
    }
    int type = expression(parser);
    switch (type) {
      case TYPE_NUM:
        emit(parser->bcode, B_PRINT_NUM);
        break;
      case TYPE_STR:
        emit(parser->bcode, B_PRINT_STR);
        break;
      default:
        parse_error(parser, "an expression was expected");
        break;
    }
    sep = 0;
  }

  if (sep == 0)
    emit(parser->bcode, B_PRINT_LN);
}

static void print_builtin(PARSER* parser, int opcode) {
  lex_next(parser->lex);
  match(parser, '(');
  numeric_expression(parser);
  match(parser, ')');
  emit(parser->bcode, opcode);
}

bool name_is_print_builtin(const char* name) {
  return STRICMP(name, "SPC") == 0 || STRICMP(name, "TAB") == 0;
}

static void randomize_statement(PARSER* parser) {
  match(parser, TOK_RANDOMIZE);
  if (eos(parser->lex))
    emit(parser->bcode, B_RAND);
  else {
    numeric_expression(parser);
    emit(parser->bcode, B_SEED);
  }
}

static void rem_statement(PARSER* parser) {
  lex_discard(parser->lex);
}

static void return_statement(PARSER* parser) {
  match(parser, TOK_RETURN);
  emit(parser->bcode, B_RETURN);
}

static void stop_statement(PARSER* parser) {
  match(parser, TOK_STOP);
  emit(parser->bcode, B_STOP);
}

static void numeric_expression(PARSER* parser) {
  if (expression(parser) != TYPE_NUM)
    parse_error_no_token(parser, "numeric expression expected");
}

static void string_expression(PARSER* parser) {
  if (expression(parser) != TYPE_STR)
    parse_error_no_token(parser, "string expression expected");
}

static void discard_expression(PARSER* parser) {
  int t = expression(parser);
  switch (t) {
    case TYPE_NUM: emit(parser->bcode, B_POP_NUM); break;
    case TYPE_STR: emit(parser->bcode, B_POP_STR); break;
  }
}

static int OR_expression(PARSER*);

static int expression(PARSER* parser) {
  return OR_expression(parser);
}

static int AND_expression(PARSER*);

static int OR_expression(PARSER* parser) {
  int type1 = AND_expression(parser);

  while (lex_token(parser->lex) == TOK_OR) {
    lex_next(parser->lex);
    int type2 = AND_expression(parser);
    if (type1 == TYPE_NUM && type2 == TYPE_NUM)
      emit(parser->bcode, B_OR);
    else
      parse_error_no_token(parser, "Invalid types for OR");
  }

  return type1;
}

static int NOT_expression(PARSER*);

static int AND_expression(PARSER* parser) {
  int type1 = NOT_expression(parser);

  while (lex_token(parser->lex) == TOK_AND) {
    lex_next(parser->lex);
    int type2 = NOT_expression(parser);
    if (type1 == TYPE_NUM && type2 == TYPE_NUM)
      emit(parser->bcode, B_AND);
    else
      parse_error_no_token(parser, "Invalid types for AND");
  }

  return type1;
}

static int relational_expression(PARSER*);

static int NOT_expression(PARSER* parser) {
  unsigned not = 0;

  if (lex_token(parser->lex) == TOK_NOT) {
    do {
      not++;
    } while (lex_next(parser->lex) == TOK_NOT);
  }

  int type = relational_expression(parser);

  if (not) {
    if (type != TYPE_NUM)
      parse_error_no_token(parser, "NOT requires a numeric value");
    if (not % 2)
      emit(parser->bcode, B_NOT);
  }

  return type;
}

static bool relop(int t) {
  return t == '=' || t == '<' || t == '>' || t == TOK_NE || t == TOK_LE || t == TOK_GE;
}

static int add_expr(PARSER*);

static int relational_expression(PARSER* parser) {
  int type1 = add_expr(parser);
  if (type1 == TYPE_ERR)
    return TYPE_ERR;
  if (relop(lex_token(parser->lex))) {
    int op = lex_token(parser->lex);
    lex_next(parser->lex);
    int type2 = add_expr(parser);
    if (type2 == TYPE_ERR)
      return TYPE_ERR;
    if (type1 != type2)
      parse_error(parser, "type mismatch in relational expression");
    if (type1 == TYPE_STR) {
      switch (op) {
        case '=': emit(parser->bcode, B_EQ_STR); break;
        case '<': emit(parser->bcode, B_LT_STR); break;
        case '>': emit(parser->bcode, B_GT_STR); break;
        case TOK_NE: emit(parser->bcode, B_NE_STR); break;
        case TOK_LE: emit(parser->bcode, B_LE_STR); break;
        case TOK_GE: emit(parser->bcode, B_GE_STR); break;
        default: parse_error_token(parser, "relational operator unsupported", op); break;
      }
      return TYPE_NUM;
    }
    assert(type1 == TYPE_NUM);
    switch (op) {
      case '=': emit(parser->bcode, B_EQ_NUM); break;
      case '<': emit(parser->bcode, B_LT_NUM); break;
      case '>': emit(parser->bcode, B_GT_NUM); break;
      case TOK_NE: emit(parser->bcode, B_NE_NUM); break;
      case TOK_LE: emit(parser->bcode, B_LE_NUM); break;
      case TOK_GE: emit(parser->bcode, B_GE_NUM); break;
      default: parse_error_token(parser, "relational operator unsupported", op); break;
    }
    return TYPE_NUM;
  }
  return type1;
}

static int mult_expr(PARSER*);

static int add_expr(PARSER* parser) {
  int type1 = mult_expr(parser);

  switch (type1) {
    case TYPE_NUM:
      while (lex_token(parser->lex) == '+' || lex_token(parser->lex) == '-') {
        int op = lex_token(parser->lex) == '+' ? B_ADD : B_SUB;
        lex_next(parser->lex);
        int type2 = mult_expr(parser);
        if (type1 != type2)
          parse_error_no_token(parser, "Additive operator type mismatch");
        emit(parser->bcode, op);
      }
      break;
    case TYPE_STR:
      while (lex_token(parser->lex) == '+') {
        lex_next(parser->lex);
        int type2 = mult_expr(parser);
        if (type1 != type2)
          parse_error_no_token(parser, "String concatenation type mismatch");
        emit(parser->bcode, B_CONCAT);
      }
      break;
  }

  return type1;
}

static int neg_expr(PARSER*);

static int mult_expr(PARSER* parser) {
  int type1 = neg_expr(parser);

  while (lex_token(parser->lex) == '*' || lex_token(parser->lex) == '/') {
    int op = lex_token(parser->lex) == '*' ? B_MUL : B_DIV;
    lex_next(parser->lex);
    int type2 = neg_expr(parser);
    if (type1 == TYPE_NUM && type2 == TYPE_NUM)
      emit(parser->bcode, op);
    else
      parse_error_no_token(parser, "Invalid types for multiplicative operator");
  }

  return type1;
}

static int power_expr(PARSER*);

static int neg_expr(PARSER* parser) {
  unsigned neg = 0;

  if (lex_token(parser->lex) == '-') {
    do {
      neg++;
    } while (lex_next(parser->lex) == '-');
  }

  int type = power_expr(parser);

  if (neg) {
    if (type != TYPE_NUM)
      parse_error_no_token(parser, "negation requires a numeric value");
    if (neg % 2)
      emit(parser->bcode, B_NEG);
  }

  return type;
}

static int primary_expression(PARSER*);

static int power_expr(PARSER* parser) {
  int type1 = primary_expression(parser);

  if (type1 == TYPE_NUM && lex_token(parser->lex) == '^') {
    lex_next(parser->lex);
    int type2 = power_expr(parser);
    if (type2 != TYPE_NUM)
      return TYPE_ERR;
    emit(parser->bcode, B_POW);
  }

  return type1;
}

static void builtin_arg(PARSER*, int type);

static int primary_expression(PARSER* parser) {
  if (lex_token(parser->lex) == TOK_NUM) {
    emit_num(parser->bcode, B_PUSH_NUM, lex_num(parser->lex));
    lex_next(parser->lex);
    return TYPE_NUM;
  }
  if (lex_token(parser->lex) == TOK_STR) {
    emit_str(parser->bcode, B_PUSH_STR, lex_word(parser->lex));
    lex_next(parser->lex);
    return TYPE_STR;
  }
  if (lex_token(parser->lex) == TOK_ID) {
    const BUILTIN* b = builtin(lex_word(parser->lex));
    if (b) {
      if (b->type == TYPE_ERR)
        parse_error(parser, "built-in function not yet implemented");
      lex_next(parser->lex);
      const char* arg = b->args;
      assert(arg != NULL); // otherwise type would be ERR, not implemented
      assert(*arg); // even if 'd' dummy argument
      if (*arg == 'd') {
        // if argument is only a dummy, allow RND, RND(), RND(0), RND("")
        if (lex_token(parser->lex) == '(') {
          lex_next(parser->lex);
          if (lex_token(parser->lex) != ')')
            discard_expression(parser);
          match(parser, ')');
        }
      }
      else {
        match(parser, '(');
        builtin_arg(parser, *arg);
        for (arg++; *arg; arg++) {
          match(parser, ',');
          builtin_arg(parser, *arg);
        }
        match(parser, ')');
      }
      emit(parser->bcode, b->opcode);
      return b->type;
    }
    unsigned name, params = 0;
    int type = identifier(parser, &name, &params);
    if (params)
      emit_param(parser->bcode, type == TYPE_STR ? B_GET_PAREN_STR : B_GET_PAREN_NUM, name, params);
    else
      emit_var(parser->bcode, type == TYPE_STR ? B_GET_SIMPLE_STR : B_GET_SIMPLE_NUM, name);
    return type;
  }
  if (lex_token(parser->lex) == '(') {
    lex_next(parser->lex);
    int t = expression(parser);
    if (t != TYPE_ERR)
      match(parser, ')');
    return t;
  }
  parse_error(parser, "expression expected");
  return TYPE_ERR;
}

static void builtin_arg(PARSER* parser, int type) {
  switch (type) {
    case 'n': numeric_expression(parser); break;
    case 's': string_expression(parser); break;
    default: parse_error(parser, "internal error: unknown argument type: '%c'", type); break;
  }
}

static int identifier(PARSER* parser, unsigned *name, unsigned *parameters) {
  int type = TYPE_ERR;

  if (lex_token(parser->lex) == TOK_ID) {
    assert(parser->names != NULL);
    *name = name_entry(parser->names, lex_word(parser->lex));
    type = string_name(lex_word(parser->lex)) ? TYPE_STR : TYPE_NUM;
  }
  match(parser, TOK_ID);

  *parameters = 0;
  if (lex_token(parser->lex) == '(') {
    lex_next(parser->lex);
    numeric_expression(parser);
    ++*parameters;
    while (lex_token(parser->lex) == ',') {
      lex_next(parser->lex);
      numeric_expression(parser);
      ++*parameters;
    }
    match(parser, ')');
  }

  assert(type == TYPE_NUM || type == TYPE_STR);
  return type;
}

static unsigned line_number(PARSER* parser) {
  if (lex_token(parser->lex) == TOK_NUM) {
    double x = lex_num(parser->lex);
    if (x > 0 && x <= (U16)(-1) && floor(x) == x) {
      lex_next(parser->lex);
      return (unsigned) x;
    }
  }
  parse_error(parser, "line number expected");
  return 0;
}
