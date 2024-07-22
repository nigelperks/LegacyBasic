// Legacy BASIC
// Basic interpreter for running 70s/80s microcomputer Basic games.
// Copyright (c) 2022-24 Nigel Perks
// Interactive command mode.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <assert.h>
#include "interactive.h"
#include "run.h"
#include "lexer.h"
#include "token.h"
#include "interrupt.h"
#include "os.h"
#include "utils.h"

static const unsigned PAGE = 24;

enum command_tokens {
  CMD_NONE,
  CMD_BYE,
  CMD_COMPILE,
  CMD_CONT,
  CMD_HELP,
  CMD_LIST,
  CMD_LOAD,
  CMD_NEW,
  CMD_RENUM,
  CMD_RUN,
  CMD_SAVE,
};

static const struct command {
  char* name;
  unsigned short len;
  short cmd;
} commands[] = {
  { "BYE", 3, CMD_BYE },
  { "COMPILE", 7, CMD_COMPILE },
  { "CONT", 4, CMD_CONT },
  { "HELP", 4, CMD_HELP },
  { "LIST", 4, CMD_LIST },
  { "LOAD", 4, CMD_LOAD },
  { "NEW", 3, CMD_NEW },
  { "RENUM", 5, CMD_RENUM },
  { "RENUMBER", 8, CMD_RENUM },
  { "RUN", 3, CMD_RUN },
  { "SAVE", 4, CMD_SAVE },
};

static int find_command(const char* cmd, unsigned len) {
  for (unsigned i = 0; i < sizeof commands / sizeof commands[0]; i++) {
    if (commands[i].len == len && STRNICMP(commands[i].name, cmd, len) == 0)
      return commands[i].cmd;
  }

  return CMD_NONE;
}

static void help(void) {
  puts("BYE                       quit to operating system");
  puts("COMPILE                   compile program to check syntax without running");
  puts("CONT                      continue program after STOP or break");
  puts("HELP                      show this help");
  puts("LIST [[start]-[end]][P]   list current file");
  puts("LOAD \"program.bas\"        load source from file");
  puts("NEW                       wipe current file from memory");
  puts("RENUM [new[,old[,inc]]]   renumber program lines");
  puts("RUN                       run current file as a Basic program");
  puts("SAVE \"program.bas\"        save current file under given name");
  puts("*DIR                      run DIR or other operating system command");
}

static bool get_line(char* cmd, unsigned cmd_size);
static void interpret(VM*, char* cmd, bool *quit);

void interact(bool keywords_anywhere, bool trace_basic, bool trace_for, bool quiet) {
  VM* vm = new_vm(keywords_anywhere, trace_basic, trace_for, /*trace_log*/ false);
  char cmd[128];
  bool quit = false;

  if (!quiet)
    puts("Type HELP to list commands\n");

  while (!quit && get_line(cmd, sizeof cmd))
    interpret(vm, cmd, &quit);

  delete_vm(vm);
}

static bool line_complete(char* cmd);

static bool get_line(char* cmd, unsigned cmd_size) {
  do {
    fputs("> ", stdout);
    fflush(stdout);
    if (fgets(cmd, cmd_size, stdin) == NULL)
      return false;
  } while (!line_complete(cmd));
  return true;
}

static bool line_complete(char* cmd) {
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

static char* skip_space(char* line);
static char* read_num(char* line, unsigned *num);
static char* demarcate_string(char* line, const char* *str);
static bool check_eol(char* line);

static void program_line(VM*, char* line);
static void immediate(VM*, char* line, bool *quit);
static void oscli(char* line);

static void interpret(VM* vm, char* line, bool *quit) {
  char* p = skip_space(line);
  if (isdigit(*p))
    program_line(vm, p);
  else if (*p == '*')
    oscli(p+1);
  else if (*p && *p != '\n')
    immediate(vm, p, quit);
}

static void program_line(VM* vm, char* line) {
  assert(line != NULL && isdigit(*line));
  unsigned num = 0;
  const char* p = read_num(line, &num);
  if (*p == '\0' || *p == '\n')
    vm_delete_source_line(vm, num);
  else {
    if (*p == ' ')
      p++;
    vm_enter_source_line(vm, num, p);
  }
}

static void command(VM*, int cmd, char* line, bool *quit);

static void immediate(VM* vm, char* line, bool *quit) {
  assert(line != NULL && !isdigit(*line));

  if (isalpha(*line)) {
    char* p = line + 1;
    while (isalpha(*p))
      p++;
    int k = find_command(line, (unsigned) (p - line));
    if (k) {
      command(vm, k, p, quit);
      return;
    }
  }

  run_immediate(vm, line);
}

static char* get_list_numbers(char* line, unsigned *start, unsigned *end);
static char* get_renum_numbers(char* line, unsigned *new, unsigned *old, unsigned *inc);
static char* get_page_flag(char* line, bool *page);
static void list(const VM*, unsigned start, unsigned end, bool page);
static void renumber(VM*, unsigned new, unsigned old, unsigned inc);

static void command(VM* vm, int cmd, char* line, bool *quit) {
  switch (cmd) {
    case CMD_BYE:
      if (check_eol(line))
        *quit = true;
      break;
    case CMD_COMPILE:
      vm_compile(vm);
      break;
    case CMD_CONT:
      if(!vm_continue(vm))
        error("Cannot continue");
      break;
    case CMD_HELP:
      help();
      break;
    case CMD_NEW:
      if (check_eol(line)) {
        vm_new_program(vm);
        vm_clear_names(vm);
      }
      break;
    case CMD_LIST: {
      unsigned start = 0, end = -1;
      bool page = false;
      line = get_list_numbers(line, &start, &end);
      line = get_page_flag(line, &page);
      if (check_eol(line))
        list(vm, start, end, page);
      break;
    }
    case CMD_LOAD: {
      const char* str;
      if ((line = demarcate_string(line, &str)) && check_eol(line))
        vm_load_source(vm, str);
      else
        error("Quoted file name expected");
      break;
    }
    case CMD_SAVE: {
      const char* str;
      if ((line = demarcate_string(line, &str)) && check_eol(line))
        vm_save_source(vm, str);
      else
        error("Quoted file name expected");
      break;
    }
    case CMD_RENUM: {
      unsigned new = 10, old = 1, inc = 10;
      line = get_renum_numbers(line, &new, &old, &inc);
      if (check_eol(line))
        renumber(vm, new, old, inc);
      break;
    }
    case CMD_RUN:
      vm_clear_values(vm);
      trap_interrupt();
      run_program(vm);
      untrap_interrupt();
      break;
    default:
      assert(0 && "unknown command");
  }
}

static char* get_list_numbers(char* p, unsigned *start, unsigned *end) {
  *start = 0;
  *end = -1;

  p = skip_space(p);

  if (isdigit(*p)) {
    p = read_num(p, start);
    p = skip_space(p);
  }

  if (*p == '-') {
    p = skip_space(p + 1);
    if (isdigit(*p))
      p = read_num(p, end);
  }

  return p;
}

static char* get_renum_numbers(char* p, unsigned *new, unsigned *old, unsigned *inc) {
  *new = 10;
  *old = 1;
  *inc = 10;

  p = skip_space(p);

  if (isdigit(*p)) {
    p = read_num(p, new);
    p = skip_space(p);
  }

  if (*p == ',') {
    p = skip_space(p + 1);
    if (isdigit(*p))
      p = read_num(p, old);
  }

  if (*p == ',') {
    p = skip_space(p + 1);
    if (isdigit(*p))
      p = read_num(p, inc);
  }

  return p;
}

static char* get_page_flag(char* line, bool *page) {
  line = skip_space(line);
  if (tolower(*line) == 'p') {
    *page = true;
    line++;
  }
  else
    *page = false;
  return line;
}

static void await_newline(void);

// List source between given BASIC line numbers.
// Pause after each page.
static void list(const VM* vm, unsigned start, unsigned end, bool page) {
  assert(vm != NULL);
  const SOURCE* src = vm_stored_source(vm);
  if (src && source_lines(src)) {
    trap_interrupt();
    unsigned count = 0;
    for (unsigned i = 0; i < source_lines(src) && !interrupted; i++) {
      const unsigned lineno = source_linenum(src, i);
      if (lineno > end)
        break;
      if (lineno >= start && lineno <= end) {
        printf("%u %s", source_linenum(src, i), source_text(src, i));
        count++;
        if (page && count % PAGE == PAGE-1 && i + 1 < source_lines(src))
          await_newline();
        else
          putchar('\n');
      }
    }
    untrap_interrupt();
  }
}

static bool takes_line_number(int token);

static void renumber(VM* vm, unsigned new_start, unsigned old_start, unsigned inc) {
  SOURCE* source = vm_stored_source(vm);
  if (source == NULL)
    return;

  LEX* lex = new_lex(/*name*/ NULL, vm_keywords_anywhere(vm));

  // create mapping of old Basic line number to new Basic line number
  // update the line number of each source line
  LINE_MAP* map = new_line_map(source_lines(source));
  unsigned new_linenum = new_start;
  unsigned prev_basic_line = 0;
  for (unsigned line = 0; line < source_lines(source); line++) {
    unsigned basic_line = source_linenum(source, line);
    if (basic_line >= old_start) {
      if (new_linenum < prev_basic_line) {
        error("Cannot renumber line %u to %u after line %u",
              basic_line, new_linenum, prev_basic_line);
        goto bail;
      }
      insert_line_mapping(map, basic_line, new_linenum);
      set_source_linenum(source, line, new_linenum);
      prev_basic_line = new_linenum;
      new_linenum += inc;
    }
    else {
      insert_line_mapping(map, basic_line, basic_line);
      prev_basic_line = basic_line;
    }
  }

  // update line numbers in source text
  for (unsigned line = 0; line < source_lines(source); line++) {
    int tok = lex_line(lex, line, source_text(source, line));
    while (tok != TOK_EOF && tok != '\n') {
      if (takes_line_number(tok)) {
        tok = lex_next(lex);
        if (tok == TOK_NUM) {
          double x = lex_num(lex);
          if (x > 0 && x <= (U16)(-1) && floor(x) == x) {
            unsigned basic_line = (unsigned) x;
            unsigned new_basic_line;
            if (lookup_line_mapping(map, basic_line, &new_basic_line)) {
              char num[16];
              sprintf(num, "%u", new_basic_line);
              if (!source_replace(source, line, lex_token_pos(lex), lex_word(lex), num)) {
                error("internal error: renumbering: replacing line number in source line");
                goto bail;
              }
              tok = lex_refresh(lex, source_text(source, line));
              if (tok != TOK_NUM) {
                error("internal error: renumbering: retokenising update source text");
                goto bail;
              }
            }
          }
        }
      }
      tok = lex_next(lex);
    }
  }

bail:
  delete_line_map(map);
  delete_lex(lex);
}

static bool takes_line_number(int token) {
      switch (token) {
        case TOK_ELSE:
        case TOK_GOSUB:
        case TOK_GOTO:
        case TOK_RESTORE:
        case TOK_THEN:
          return true;
      }
      return false;
}

static void await_newline(void) {
  fflush(stdout);
  int c;
  while ((c = getchar()) != EOF && c != '\n')
    ;
}

static void oscli(char* line) {
  line = skip_space(line);
  if (*line == '\0' || *line == '\n')
    return;
  system(line);
}

static bool check_eol(char* line) {
  const char* p = skip_space(line);
  if (*p && *p != '\n') {
    error("Command syntax error");
    return false;
  }
  return true;
}

static char* skip_space(char* s) {
  while (*s == ' ' || *s == '\t')
    s++;
  return s;
}

static char* read_num(char* line, unsigned *num) {
  assert(line != NULL && isdigit(*line));
  *num = 0;
  do {
    *num = 10 * *num + *line - '0';
    line++;
  } while (isdigit(*line));
  return line;
}

static char* demarcate_string(char* line, const char* *str) {
  line = skip_space(line);
  if (*line != '\"')
    return NULL;
  line++;
  *str = line;
  while (*line && *line != '\n' && *line != '\"')
    line++;
  if (*line == '\"') {
    *line = '\0';
    return line + 1;
  }
  *line = '\0';
  return line;
}
