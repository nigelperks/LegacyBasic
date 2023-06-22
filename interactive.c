#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include "interactive.h"
#include "run.h"
#include "parse.h"
#include "lexer.h"
#include "source.h"
#include "token.h"
#include "utils.h"
#include "interrupt.h"

enum command_tokens {
  CMD_BYE = FIRST_KEYWORD_TOKEN,
  CMD_LIST,
  CMD_LOAD,
  CMD_NEW,
  CMD_RUN,
  CMD_SAVE,
};

static const KEYWORD commands[] = {
  { "BYE", 3, CMD_BYE },
  { "LIST", 4, CMD_LIST },
  { "LOAD", 4, CMD_LOAD },
  { "NEW", 3, CMD_NEW },
  { "RUN", 3, CMD_RUN },
  { "SAVE", 4, CMD_SAVE },
  { NULL, 0, TOK_NONE }
};

// immediate mode environment
typedef struct {
  VM* vm;
  LEX* lex;
} IMMED;

static IMMED* new_immediate(bool keywords_anywhere, bool trace_basic, bool trace_for) {
  IMMED* p = emalloc(sizeof *p);
  p->vm = new_vm(keywords_anywhere, trace_basic, trace_for);
  p->lex = new_lex(NULL, commands, true);
  return p;
}

static void delete_immediate(IMMED* p) {
  if (p) {
    delete_vm(p->vm);
    delete_lex(p->lex);
    efree(p);
  }
}

static void prompt(void);

static void interpret(IMMED*, const char* cmd, bool *quit);

static bool line_complete(char* cmd);

void interact(bool keywords_anywhere, bool trace_basic, bool trace_for) {
  IMMED* imm = new_immediate(keywords_anywhere, trace_basic, trace_for);

  for (;;) {
    prompt();
    char cmd[256];
    if (fgets(cmd, sizeof cmd, stdin) == NULL)
      break;
    if (line_complete(cmd)) {
      bool quit = false;
      interpret(imm, cmd, &quit);
      if (quit)
        break;
    }
  }

  delete_immediate(imm);
}

static void prompt(void) {
  fputs("> ", stdout);
  fflush(stdout);
}

static bool line_complete(char* cmd) {
  assert(cmd != NULL);
  size_t len = strlen(cmd);
  if (len == 0)
    return false;
  if (cmd[len-1] == '\n') {
    cmd[len-1] = '\0';
    return true;
  }
  error("Command line too long");
  int c;
  while ((c = getchar()) != EOF && c != '\n')
    ;
  return false;
}

static void immediate(IMMED*, const char* const cmd, bool *quit);
static void program_line(IMMED*, unsigned num, const char* const line);

static void interpret(IMMED* imm, const char* line, bool *quit) {
  int tok = lex_line(imm->lex, 0, line);
  if (tok == '\n')
    ;
  else if (tok == TOK_NUM) {
    unsigned num;
    if (lex_unsigned(imm->lex, &num))
      program_line(imm, num, line);
    else
      error("Invalid line number");
  }
  else
    immediate(imm, line, quit);
}

static void program_line(IMMED* imm, unsigned num, const char* const cmd) {
  if (lex_peek(imm->lex) == '\n')
    vm_delete_source_line(imm->vm, num);
  else {
    const char* p = lex_remaining(imm->lex);
    if (*p == ' ')
      p++;
    vm_enter_source_line(imm->vm, num, p);
  }
}

static void command(IMMED*, int token, bool *quit);
static void immediate_statement(IMMED*, const char*);

static void immediate(IMMED* imm, const char* const cmd, bool *quit) {
  assert(cmd != NULL && cmd[0] != '\0');
  int tok = lex_token(imm->lex);
  assert(tok != TOK_EOF);
  if (tok == '\n')
    ;
  else if (tok >= FIRST_KEYWORD_TOKEN)
    command(imm, tok, quit);
  else
    immediate_statement(imm, cmd);
}

static bool check_eol(LEX*);

static const char SYNTAX[] = "Command syntax error";

static bool get_list_numbers(LEX*, unsigned *start, unsigned *end);
static void list(const VM*, unsigned start, unsigned end);

static void command(IMMED* imm, int token, bool *quit) {
  switch (token) {
    case CMD_BYE:
      if (check_eol(imm->lex))
        *quit = true;
      break;
    case CMD_NEW:
      if (check_eol(imm->lex)) {
        vm_new_program(imm->vm);
        vm_new_environment(imm->vm);
      }
      break;
    case CMD_LIST: {
      unsigned start = 0, end = -1;
      if (get_list_numbers(imm->lex, &start, &end))
        list(imm->vm, start, end);
      break;
    }
    case CMD_LOAD:
      if (lex_next(imm->lex) == TOK_STR) {
        SOURCE* source = load_source_file(lex_word(imm->lex));
        if (source && check_eol(imm->lex))
          vm_take_source(imm->vm, source);
      }
      else
        error("Quoted file name expected");
      break;
    case CMD_SAVE: {
      if (lex_next(imm->lex) != TOK_STR) {
        error("Quoted file name expected");
        return;
      }
      char* name = estrdup(lex_word(imm->lex));
      if (!check_eol(imm->lex))
        return;
      FILE* fp = fopen(name, "w");
      if (fp == NULL) {
        error("Cannot create file: %s", name);
        return;
      }
      for (unsigned i = 0; i < vm_source_lines(imm->vm); i++)
        fprintf(fp, "%u %s\n", vm_source_linenum(imm->vm, i), vm_source_text(imm->vm, i));
      fclose(fp);
      break;
    }
    case CMD_RUN:
      vm_clear_environment(imm->vm);
      trap_interrupt();
      run_program(imm->vm);
      untrap_interrupt();
      break;
    default:
      assert(0 && "unknown command");
  }
}

static bool get_list_numbers(LEX* lex, unsigned *start, unsigned *end) {
  *start = 0;
  *end = -1;

      lex_next(lex);
      if (lex_unsigned(lex, start)) {
        if (lex_next(lex) == '-') {
          lex_next(lex);
          if (lex_unsigned(lex, end))
            lex_next(lex);
          else if (lex_token(lex) != '\n') {
            error(SYNTAX);
            return false;
          }
        }
        else if (lex_token(lex) == '\n')
          *end = *start;
        else {
          error(SYNTAX);
          return false;
        }
      }
      else if (lex_token(lex) == '-') {
        lex_next(lex);
        if (!lex_unsigned(lex, end)) {
          error(SYNTAX);
          return false;
        }
        lex_next(lex);
      }
      if (lex_token(lex) != '\n') {
        error(SYNTAX);
        return false;
      }

  return true;
}

static void await_newline(void);

// List source between given BASIC line numbers.
// Pause after each page.
static void list(const VM* vm, unsigned start, unsigned end) {
  assert(vm != NULL);
  if (vm_source_lines(vm)) {
    trap_interrupt();
    unsigned count = 0;
    const unsigned PAGE = 22;
    for (unsigned i = 0; i < vm_source_lines(vm) && !interrupted; i++) {
      const unsigned lineno = vm_source_linenum(vm, i);
      if (lineno >= start && lineno <= end) {
        printf("%u %s", vm_source_linenum(vm, i), vm_source_text(vm, i));
        count++;
        if (count % PAGE == PAGE-1 && i + 1 < vm_source_lines(vm))
          await_newline();
        else
          putchar('\n');
      }
    }
    untrap_interrupt();
  }
}

static void await_newline(void) {
  fflush(stdout);
  int c;
  while ((c = getchar()) != EOF && c != '\n')
    ;
}

static bool check_eol(LEX* lex) {
  if (lex_next(lex) != '\n') {
    error(SYNTAX);
    return false;
  }
  return true;
}

static void immediate_statement(IMMED* imm, const char* s) {
  SOURCE* source = wrap_source_text(s);
  trap_interrupt();
  run_immediate(imm->vm, source, false);
  untrap_interrupt();
  delete_source(source);
}
