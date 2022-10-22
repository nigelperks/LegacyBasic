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
  SOURCE* source;
  ENV* env;
  LEX* lex;
} IMMED;

static IMMED* new_immediate(void) {
  IMMED* p = emalloc(sizeof *p);
  p->source = new_source(NULL);
  p->env = new_environment();
  p->lex = new_lex(NULL, commands, true);
  return p;
}

static void delete_immediate(IMMED* p) {
  if (p) {
    delete_source(p->source);
    delete_environment(p->env);
    delete_lex(p->lex);
    efree(p);
  }
}

static void error(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap); 
  va_end(ap);
  putchar('\n');
}

static void interpret(IMMED*, const char* cmd, bool *quit);

static bool line_complete(char* cmd, bool *eof);

void interact(void) {
  IMMED* imm = new_immediate();

  for (;;) {
    fputs("> ", stdout);
    fflush(stdout);
    char cmd[256];
    if (fgets(cmd, sizeof cmd, stdin) == NULL)
      break;
    bool eof = false;
    if (line_complete(cmd, &eof)) {
      bool quit = false;
      interpret(imm, cmd, &quit);
      if (quit)
        break;
    }
    else if (eof)
      break;
  }

  delete_immediate(imm);
}

static bool line_complete(char* cmd, bool *eof) {
  assert(cmd != NULL && eof != NULL);
  *eof = false;
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
  if (c == EOF)
    *eof = true;
  return false;
}

static void immediate(IMMED*, const char* const cmd, bool *quit);
static void program_line(IMMED*, unsigned num, const char* const line);

static void interpret(IMMED* imm, const char* line, bool *quit) {
  int tok = lex_line(imm->lex, 0, line);
  if (tok == TOK_NUM) {
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
  if (lex_peek(imm->lex) == '\n') {
    unsigned i;
    if (find_source_linenum(imm->source, num, &i))
      delete_source_line(imm->source, i);
  }
  else {
    const char* p = lex_remaining(imm->lex);
    if (*p == ' ')
      p++;
    enter_source_line(imm->source, num, p);
  }
}

static void command(IMMED*, int token, bool *quit);
static void immediate_statement(ENV*, const char*);

static void immediate(IMMED* imm, const char* const cmd, bool *quit) {
  assert(cmd != NULL && cmd[0] != '\0');
  int tok = lex_token(imm->lex);
  assert(tok != TOK_EOF);
  if (tok == '\n')
    ;
  else if (tok >= FIRST_KEYWORD_TOKEN)
    command(imm, tok, quit);
  else
    immediate_statement(imm->env, cmd);
}

static bool check_eol(LEX*);

static const char SYNTAX[] = "Command syntax error";

static bool get_list_numbers(LEX*, unsigned *start, unsigned *end);
static void list(const SOURCE*, unsigned start, unsigned end);

static void command(IMMED* imm, int token, bool *quit) {
  switch (token) {
    case CMD_BYE:
      if (check_eol(imm->lex))
        *quit = true;
      break;
    case CMD_NEW:
      if (check_eol(imm->lex)) {
        clear_environment(imm->env);
        clear_source(imm->source);
      }
      break;
    case CMD_LIST: {
      unsigned start = 0, end = -1;
      if (get_list_numbers(imm->lex, &start, &end)) {
        list(imm->source, start, end);
      }
      break;
    }
    case CMD_LOAD:
      if (lex_next(imm->lex) == TOK_STR) {
        SOURCE* src = load_source_file(lex_word(imm->lex));
        if (src && check_eol(imm->lex)) {
          delete_source(imm->source);
          imm->source = src;
        }
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
      for (unsigned i = 0; i < source_lines(imm->source); i++)
        fprintf(fp, "%u %s\n", source_linenum(imm->source, i), source_text(imm->source, i));
      fclose(fp);
      break;
    }
    case CMD_RUN: {
      ENV* env = new_environment();
      bool keywords_anywhere = false;
      BCODE* bcode = parse_source(imm->source, env->names, keywords_anywhere);
      if (bcode) {
        trap_interrupt();
        run(bcode, env, imm->source, false, false, false);
        untrap_interrupt();
        delete_bcode(bcode);
      }
      delete_environment(env);
      break;
    }
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
static void list(const SOURCE* source, unsigned start, unsigned end) {
      trap_interrupt();
      unsigned count = 0;
      const unsigned PAGE = 22;
      for (unsigned i = 0; i < source->used && !interrupted; i++) {
        const unsigned lineno = source_linenum(source, i);
        if (lineno >= start && lineno <= end) {
          print_source_line(source, i, stdout);
          count++;
          if (count % PAGE == PAGE-1 && i + 1 < source->used)
            await_newline();
          else
            putchar('\n');
        }
      }
      trap_interrupt();
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

static void immediate_statement(ENV* env, const char* s) {
  bool keywords_anywhere = true;
  BCODE* bc = parse_text(s, env->names, NULL, keywords_anywhere);
  if (bc) {
    SOURCE* src = wrap_source_text(s);
    trap_interrupt();
    run(bc, env, src, false, false, false);
    untrap_interrupt();
    delete_source(src);
    delete_bcode(bc);
    clear_code_dependent_environment(env);
  }
}
