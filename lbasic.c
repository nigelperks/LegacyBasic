// Legacy BASIC
// Basic interpreter for running 70s/80s microcomputer Basic games.
// Copyright (c) 2022 Nigel Perks

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "source.h"
#include "parse.h"
#include "run.h"
#include "utils.h"

#define TITLE "Legacy BASIC"
#define VERSION "1.0.1"
#define COPYRIGHT "Copyright (c) 2022 Nigel Perks"

#ifdef UNIT_TEST
static void unit_tests(void);
#endif

static void print_version(void);
static void help(bool full);

int main(int argc, char* argv[]) {
  enum { LIST, PARSE, CODE, RUN, TEST } mode = RUN;
  const char* name = NULL;
  bool trace_basic = false;
  bool trace_for = false;
  bool recognise_keyword_prefixes = false;
  bool randomize = false;
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
    else if (strcmp(arg, "--parse") == 0 || strcmp(arg, "-p") == 0)
      mode = PARSE;
    else if (strcmp(arg, "--code") == 0 || strcmp(arg, "-c") == 0)
      mode = CODE;
    else if (strcmp(arg, "--run") == 0 || strcmp(arg, "-r") == 0)
      mode = RUN;
#ifdef UNIT_TEST
    else if (strcmp(arg, "--unit-tests") == 0 || strcmp(arg, "-unittest") == 0)
      mode = TEST;
#endif
    else if (strcmp(arg, "--report-memory") == 0 || strcmp(arg, "-m") == 0)
      report_memory = true;
    else if (strcmp(arg, "--trace-basic") == 0 || strcmp(arg, "-t") == 0)
      trace_basic = true;
    else if (strcmp(arg, "--trace-for") == 0 || strcmp(arg, "-f") == 0)
      trace_for = true;
    else if (strcmp(arg, "--keywords-anywhere") == 0 || strcmp(arg, "-k") == 0)
      recognise_keyword_prefixes = true;
    else if (strcmp(arg, "--randomize") == 0 || strcmp(arg, "-z") == 0)
      randomize = true;
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

  if (name == NULL && mode != TEST)
    help(false);

  if (!quiet)
    print_version();

#ifdef UNIT_TEST
  if (mode == TEST)
    unit_tests();
  else
#endif
  {
    SOURCE* source = load_source_file(name);
    if (mode == LIST) {
      for (unsigned i = 0; i < source_lines(source); i++)
        printf("%5u %s\n", source_linenum(source, i), source_text(source, i));
    }
    else {
      BCODE* bcode = parse(source, recognise_keyword_prefixes);
      switch (mode) {
        case CODE:
          print_bcode(bcode, stdout);
          break;
        case RUN:
          run(bcode, trace_basic, trace_for, randomize);
          break;
      }
      delete_bcode(bcode);
    }
    delete_source(source);
  }

  if (report_memory) {
    printf("malloc: %10lu\n", malloc_count);
    printf("free:   %10lu\n", free_count);
  }

  return 0;
}

static void print_version() {
  printf("%s %s %s\n\n", TITLE, VERSION, COPYRIGHT);
}

static void help(bool full) {
  print_version();

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

#ifdef UNIT_TEST

#include "CuTest.h"

CuSuite* utils_test_suite(void);
CuSuite* token_test_suite(void);
CuSuite* stringlist_test_suite(void);
CuSuite* source_test_suite(void);
CuSuite* lexer_test_suite(void);
CuSuite* bcode_test_suite(void);
CuSuite* emit_test_suite(void);

static void unit_tests(void) {
  CuString* output = CuStringNew();
  CuSuite* suite = CuSuiteNew();

  CuSuiteAddSuite(suite, utils_test_suite());
  CuSuiteAddSuite(suite, token_test_suite());
  CuSuiteAddSuite(suite, stringlist_test_suite());
  CuSuiteAddSuite(suite, source_test_suite());
  CuSuiteAddSuite(suite, lexer_test_suite());
  CuSuiteAddSuite(suite, bcode_test_suite());
  CuSuiteAddSuite(suite, emit_test_suite());

  CuSuiteRun(suite);
  CuSuiteSummary(suite, output);
  CuSuiteDetails(suite, output);
  puts(output->buffer);
}
#endif