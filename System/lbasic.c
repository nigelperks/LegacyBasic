// Legacy BASIC
// Basic interpreter for running 70s/80s microcomputer Basic games.
// Copyright (c) 2022-24 Nigel Perks
// Main function of executable for user to start Legacy Basic:
// use the interactive monitor or run a specified Basic program.

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "options.h"
#include "interactive.h"
#include "source.h"
#include "run.h"
#include "parse.h"
#include "lexer.h"
#include "token.h"
#include "builtin.h"
#include "os.h"
#include "utils.h"
#include "stringuniq.h"
#include "symbol.h"
#include "init.h"

// These attributes are declared in C source instead of being generated
// because it better supports both CMake and development builds.
#define TITLE "Legacy Basic"
#define VERSION_MAJOR 3
#define VERSION_MINOR 2
#define VERSION_PATCH 0
#define COPYRIGHT "Copyright (c) 2022-24 Nigel Perks"

#ifdef UNIT_TEST
static int unit_tests(void);
#endif

static void print_version(void);
static void report_memory(void);

static void process_file(const Options*);

int main(int argc, char* argv[]) {
#ifdef LINUX
  progname = "legacy-basic";
#else
  progname = "LegacyBasic.exe";
#endif

  Options opt;

  init_options(&opt);
  parse_options(&opt, argv + 1);

  if (opt.print_version) {
    print_version();
    exit(EXIT_FAILURE);
  }

  if (!opt.quiet)
    print_version();

  init_keywords();

#ifdef UNIT_TEST
  if (opt.mode == TEST_MODE) {
    int rc = unit_tests();
    deinit_keywords();
    report_memory();
    exit(rc);
  }
#endif

  if (opt.file_name == NULL) {
    if (opt.mode != NO_MODE)
      fatal("invalid option for interactive mode\n");
    interact(opt.keywords_anywhere, opt.trace_basic, opt.trace_for, opt.quiet);
  }
  else
    process_file(&opt);

  deinit_keywords();

  if (opt.report_memory)
    report_memory();

  return 0;
}

static void print_version(void) {
  printf("%s %u.%u.%u %s\n\n", TITLE, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, COPYRIGHT);
}

static void report_memory(void) {
  putchar('\n');
  printf("malloc: %10lu\n", malloc_count);
  printf("free:   %10lu\n", free_count);
}

static void list_file(const char* file_name);
static void list_names(const char* file_name, bool crunched);

static void process_file(const Options* opt) {
  assert(opt != NULL && opt->file_name != NULL);

  // NO_MODE, LIST_MODE, LIST_NAMES_MODE, PARSE_MODE, CODE_MODE, RUN_MODE, TEST_MODE

  if (opt->mode == LIST_MODE) {
    list_file(opt->file_name);
    return;
  }

  if (opt->mode == LIST_NAMES_MODE) {
    list_names(opt->file_name, opt->keywords_anywhere);
    return;
  }

  if (opt->mode == PARSE_MODE || opt->mode == CODE_MODE) {
    SOURCE* source = load_source_file(opt->file_name);
    if (source) {
      SYMTAB* st = new_symbol_table();
      init_builtins(st);
      BCODE* bcode = parse_source(source, st, opt->keywords_anywhere);
      if (bcode == NULL)
        exit(EXIT_FAILURE);
      if (opt->mode == CODE_MODE) {
        for (unsigned i = 0; i < bcode->used; i++)
          print_binst(bcode->inst + i, i, source, st, stdout);
      }
      delete_bcode(bcode);
      delete_symbol_table(st);
      delete_source(source);
    }
    return;
  }

  assert(opt->mode == RUN_MODE || opt->mode == NO_MODE);

  VM* vm = new_vm(opt->keywords_anywhere, opt->trace_basic, opt->trace_for, opt->trace_log);

  if (vm_load_source(vm, opt->file_name)) {
#if HAS_TIMER
    TIMER timer;
    start_timer(&timer);
#endif
    run_program(vm);
#if HAS_TIMER
    stop_timer(&timer);
    if (opt->report_time)
      printf("Microseconds elapsed: %lld\n", elapsed_usec(&timer));
#endif
  }

  delete_vm(vm);
}

static void list_file(const char* file_name) {
  SOURCE* source = load_source_file(file_name);
  if (source) {
    for (unsigned i = 0; i < source_lines(source); i++)
      printf("%5u %s\n", source_linenum(source, i), source_text(source, i));
    delete_source(source);
  }
}

static void print_name(const char* name);

static void list_names(const char* file_name, bool crunched) {
  SOURCE* source = load_source_file(file_name);
  if (source) {
    LEX* lex = new_lex(file_name, crunched);
    UNIQUE_STRINGS* names = new_unique_strings();

    for (unsigned i = 0; i < source_lines(source); i++) {
      lex_line(lex, source_linenum(source, i), source_text(source, i));
      int t = lex_token(lex);
      while (t != '\n' && t != TOK_REM) {
        if (t == TOK_ID)
          insert_unique_string(names, lex_word(lex));
        t = lex_next(lex);
      }
    }

    traverse_unique_strings(names, print_name);

    delete_unique_strings(names);
    delete_lex(lex);
    delete_source(source);
  }
}

static void print_name(const char* name) {
  char type = ' ';
  if (builtin(name))
    type = '*';
  else if (name_is_print_builtin(name))
    type = '=';
  printf("%c %s\n", type, name);
}

#ifdef UNIT_TEST

#include "CuTest.h"

CuSuite* utils_test_suite(void);
CuSuite* token_test_suite(void);
CuSuite* stringlist_test_suite(void);
CuSuite* stringuniq_test_suite(void);
CuSuite* source_test_suite(void);
CuSuite* lexer_test_suite(void);
CuSuite* linemap_test_suite(void);
CuSuite* bcode_test_suite(void);
CuSuite* emit_test_suite(void);
CuSuite* arrays_test_suite(void);
CuSuite* symbol_test_suite(void);
CuSuite* run_test_suite(void);

static int unit_tests(void) {
  CuString* output = CuStringNew();
  CuSuite* suite = CuSuiteNew();

  CuSuiteAddSuite(suite, utils_test_suite());
  CuSuiteAddSuite(suite, token_test_suite());
  CuSuiteAddSuite(suite, stringlist_test_suite());
  CuSuiteAddSuite(suite, stringuniq_test_suite());
  CuSuiteAddSuite(suite, source_test_suite());
  CuSuiteAddSuite(suite, lexer_test_suite());
  CuSuiteAddSuite(suite, linemap_test_suite());
  CuSuiteAddSuite(suite, bcode_test_suite());
  CuSuiteAddSuite(suite, emit_test_suite());
  CuSuiteAddSuite(suite, arrays_test_suite());
  CuSuiteAddSuite(suite, symbol_test_suite());
  CuSuiteAddSuite(suite, run_test_suite());

  CuSuiteRun(suite);
  int failed = suite->failCount;
  CuSuiteSummary(suite, output);
  CuSuiteDetails(suite, output);
  puts(output->buffer);
  return failed;
}
#endif
