// Legacy BASIC
// Basic interpreter for running 70s/80s microcomputer Basic games.
// Copyright (c) 2022-24 Nigel Perks
// Interactive command mode.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include "interactive.h"
#include "run.h"
#include "interrupt.h"
#include "os.h"
#include "utils.h"

static const unsigned PAGE = 24;

enum command_tokens {
  CMD_NONE,
  CMD_BYE,
  CMD_HELP,
  CMD_LIST,
  CMD_LOAD,
  CMD_NEW,
  CMD_RUN,
  CMD_SAVE,
};

static const struct command {
  char* name;
  unsigned short len;
  short cmd;
} commands[] = {
  { "BYE", 3, CMD_BYE },
  { "HELP", 4, CMD_HELP },
  { "LIST", 4, CMD_LIST },
  { "LOAD", 4, CMD_LOAD },
  { "NEW", 3, CMD_NEW },
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
  puts("HELP                      show this help");
  puts("LIST [[start]-[end]][P]   list current file");
  puts("LOAD \"program.bas\"        load source from file");
  puts("NEW                       wipe current file from memory");
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
static char* get_page_flag(char* line, bool *page);
static void list(const VM*, unsigned start, unsigned end, bool page);

static void command(VM* vm, int cmd, char* line, bool *quit) {
  switch (cmd) {
    case CMD_BYE:
      if (check_eol(line))
        *quit = true;
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
