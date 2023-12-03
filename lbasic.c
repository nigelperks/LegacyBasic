// Legacy BASIC
// Basic interpreter for running 70s/80s microcomputer Basic games.
// Copyright (c) 2022-3 Nigel Perks

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "config.h"
#include "source.h"
#include "parse.h"
#include "lexer.h"
#include "parse.h"
#include "token.h"
#include "builtin.h"
#include "run.h"
#include "interactive.h"
#include "stringuniq.h"
#include "utils.h"

#ifdef UNIT_TEST
static int unit_tests(void);
#endif

static void print_version(void);
static void help(bool full);
static void list_file(const char* name);
static void list_names(const char* name, bool crunched);
static void compile_file(int mode, const char* name, bool keywords_anywhere,
                         bool trace_basic, bool trace_for, bool trace_log);

enum mode { LIST, LIST_NAMES, PARSE, CODE, RUN, TEST };

int main(int argc, char* argv[]) {
  enum mode mode = RUN;
  const char* name = NULL;
  bool trace_basic = false;
  bool trace_for = false;
  bool trace_log = false;
  bool keywords_anywhere = false;
  bool quiet = false;
  bool report_memory = false;

  progname = "LegacyBasic.exe";

  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0 || strcmp(arg, "-?") == 0 || strcmp(arg, "/?") == 0)
      help(false);
    if (strcmp(arg, "--help-full") == 0 || strcmp(arg, "-hh") == 0)
      help(true);
    else if (strcmp(arg, "--list") == 0 || strcmp(arg, "-l") == 0)
      mode = LIST;
    else if (strcmp(arg, "--list-names") == 0 || strcmp(arg, "-n") == 0)
      mode = LIST_NAMES;
    else if (strcmp(arg, "--parse") == 0 || strcmp(arg, "-p") == 0)
      mode = PARSE;
    else if (strcmp(arg, "--code") == 0 || strcmp(arg, "-c") == 0)
      mode = CODE;
    else if (strcmp(arg, "--run") == 0 || strcmp(arg, "-r") == 0)
      mode = RUN;
#ifdef UNIT_TEST
    else if (strcmp(arg, "--unit-tests") == 0 || strcmp(arg, "-unittest") == 0)
      mode = TEST, report_memory = true;
#endif
    else if (strcmp(arg, "--report-memory") == 0 || strcmp(arg, "-m") == 0)
      report_memory = true;
    else if (strcmp(arg, "--trace-basic") == 0 || strcmp(arg, "-t") == 0)
      trace_basic = true;
    else if (strcmp(arg, "--trace-for") == 0 || strcmp(arg, "-f") == 0)
      trace_for = true;
    else if (strcmp(arg, "--trace-log") == 0 || strcmp(arg, "-g") == 0)
      trace_log = true;
    else if (strcmp(arg, "--keywords-anywhere") == 0 || strcmp(arg, "-k") == 0)
      keywords_anywhere = true;
    else if (strcmp(arg, "--randomize") == 0 || strcmp(arg, "-z") == 0)
      srand((unsigned)time(NULL));
    else if (strcmp(arg, "--quiet") == 0 || strcmp(arg, "-q") == 0)
      quiet = true;
    else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0)
      print_version(), exit(EXIT_FAILURE);
    else if (arg[0] == '-')
      fatal("unrecognised option: %s\n", arg);
    else if (name == NULL)
      name = arg;
    else
      fatal("unexpected argument: %s\n", arg);
  }

  if (!quiet)
    print_version();

  int rc = 0;

#ifdef UNIT_TEST
  if (mode == TEST)
    rc = unit_tests();
  else
#endif
  if (name == NULL)
    interact(keywords_anywhere, trace_basic, trace_for);
  else if (mode == LIST)
    list_file(name);
  else if (mode == LIST_NAMES)
    list_names(name, keywords_anywhere);
  else
    compile_file(mode, name, keywords_anywhere, trace_basic, trace_for, trace_log);

  if (report_memory) {
    putchar('\n');
    printf("malloc: %10lu\n", malloc_count);
    printf("free:   %10lu\n", free_count);
  }

  return rc;
}

static void print_version() {
  printf("%s %u.%u.%u %s\n\n", TITLE, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, COPYRIGHT);
}

static void help(bool full) {
  printf("Usage: %s [options] name.bas\n\n", progname);

  puts("--code, -c");
  if (full)
    puts("    List translated intermediate code (B-code) program.\n");

  puts("--help, -h");
  if (full)
    puts("    Show program usage and list options.\n");

  puts("--help-full, -hh");
  if (full)
    puts("    Show program usage and explain all options.\n");

  puts("--keywords-anywhere, -k");
  if (full)
    puts("    Recognise BASIC keywords anywhere outside a string, crunched with\n"
         "    other names, not needing spaces between them.\n");

  puts("--list, -l");
  if (full)
    puts("    List the source program. This checks that line numbers are distinct\n"
         "    and in sequence, and that Legacy Basic can load the program, without\n"
         "    running it or checking for syntax errors.\n");

  puts("--list-names, -n");
  if (full)
    puts("    List the names in the source program. Flag the names of built-in\n"
         "    functions (*) and printing operators (=). The unflagged names are\n"
         "    user-defined names. If the interpreter considers a name user-\n"
         "    defined, it will not be interpreted as a built-in.\n");

  puts("--parse, -p");
  if (full)
    puts("    Parse the specified BASIC program without running it, to find\n"
         "    syntax errors or unsupported constructs.\n");

  puts("--quiet, -q");
  if (full)
    puts("    Suppress version information when running a BASIC program.\n");

  puts("--randomize, -z");
  if (full)
    puts("    Randomize the random number generator, so that RND produces a\n"
         "    different sequence of numbers in each run.\n");

  puts("--report-memory, -m");
  if (full)
    puts("    On exit, print the number of memory blocks allocated and released.\n"
         "    For debugging the interpreter.\n");

  puts("--run, -r");
  if (full)
    puts("    Run the specified BASIC program. This is the default option.\n");

  puts("--trace-basic, -t");
  if (full)
    puts("    Trace BASIC line numbers executed at runtime. Equivalent to TRON and\n"
         "    TRACE ON in some BASICs.\n");

  puts("--trace-for, -f");
  if (full)
    puts("    Print information about FOR loops at runtime.\n"
         "    For debugging the interpreter.\n");

#ifdef UNIT_TEST
  puts("--unit-tests, -unittest");
  if (full)
    puts("    Run unit tests, using the CuTest framework.\n");
#endif

  puts("--version, -v");
  if (full)
    puts("    Print version information and exit.\n");

  exit(EXIT_FAILURE);
}

static void list_file(const char* name) {
  SOURCE* source = load_source_file(name);
  if (source == NULL)
    exit(EXIT_FAILURE);
  for (unsigned i = 0; i < source_lines(source); i++)
    printf("%5u %s\n", source_linenum(source, i), source_text(source, i));
}

static void print_name(const char* name) {
  char type = ' ';
  if (builtin(name))
    type = '*';
  else if (name_is_print_builtin(name))
    type = '=';
  printf("%c %s\n", type, name);
}

static void list_names(const char* file_name, bool crunched) {
  SOURCE* source = load_source_file(file_name);
  if (source == NULL)
    exit(EXIT_FAILURE);

  LEX* lex = new_lex(source_name(source), basic_keywords, crunched);

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

static void compile_file(int mode, const char* name, bool keywords_anywhere,
                         bool trace_basic, bool trace_for, bool trace_log) {
  SOURCE* source = load_source_file(name);
  if (source == NULL)
    exit(EXIT_FAILURE);

  VM* vm = new_vm(keywords_anywhere, trace_basic, trace_for, trace_log);
  vm_take_source(vm, source);

  switch (mode) {
    case CODE:
      vm_print_bcode(vm);
      break;
    case RUN:
      run_program(vm);
      break;
  }

  delete_vm(vm);
}

#ifdef UNIT_TEST

#include "CuTest.h"

CuSuite* utils_test_suite(void);
CuSuite* token_test_suite(void);
CuSuite* stringlist_test_suite(void);
CuSuite* stringuniq_test_suite(void);
CuSuite* source_test_suite(void);
CuSuite* lexer_test_suite(void);
CuSuite* bcode_test_suite(void);
CuSuite* emit_test_suite(void);
CuSuite* arrays_test_suite(void);
CuSuite* keyword_test_suite(void);
CuSuite* paren_test_suite(void);

static int unit_tests(void) {
  CuString* output = CuStringNew();
  CuSuite* suite = CuSuiteNew();

  CuSuiteAddSuite(suite, utils_test_suite());
  CuSuiteAddSuite(suite, token_test_suite());
  CuSuiteAddSuite(suite, stringlist_test_suite());
  CuSuiteAddSuite(suite, stringuniq_test_suite());
  CuSuiteAddSuite(suite, source_test_suite());
  CuSuiteAddSuite(suite, lexer_test_suite());
  CuSuiteAddSuite(suite, bcode_test_suite());
  CuSuiteAddSuite(suite, emit_test_suite());
  CuSuiteAddSuite(suite, arrays_test_suite());
  CuSuiteAddSuite(suite, keyword_test_suite());
  CuSuiteAddSuite(suite, paren_test_suite());

  CuSuiteRun(suite);
  int failed = suite->failCount;
  CuSuiteSummary(suite, output);
  CuSuiteDetails(suite, output);
  puts(output->buffer);
  return failed;
}
#endif
