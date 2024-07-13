// Legacy BASIC
// Basic interpreter for running 70s/80s microcomputer Basic games.
// Copyright (c) 2022-24 Nigel Perks
// Command line options.

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "options.h"
#include "os.h"
#include "utils.h"

void init_options(Options* opt) {
  memset(opt, 0, sizeof *opt);
}

static void help(bool full);

void parse_options(Options* opt, const char* argv[]) {
  for (const char* arg = *argv; arg; arg = *++argv) {
    // Help
    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0 || strcmp(arg, "-?") == 0 || strcmp(arg, "/?") == 0)
      help(false);
    if (strcmp(arg, "--help-full") == 0 || strcmp(arg, "-hh") == 0)
      help(true);
    // Modes
    else if (strcmp(arg, "--list") == 0 || strcmp(arg, "-l") == 0)
      opt->mode = LIST_MODE;
    else if (strcmp(arg, "--list-names") == 0 || strcmp(arg, "-n") == 0)
      opt->mode = LIST_NAMES_MODE;
    else if (strcmp(arg, "--parse") == 0 || strcmp(arg, "-p") == 0)
      opt->mode = PARSE_MODE;
    else if (strcmp(arg, "--code") == 0 || strcmp(arg, "-c") == 0)
      opt->mode = CODE_MODE;
    else if (strcmp(arg, "--run") == 0 || strcmp(arg, "-r") == 0)
      opt->mode = RUN_MODE;
#ifdef UNIT_TEST
    else if (strcmp(arg, "--unit-tests") == 0 || strcmp(arg, "-unittest") == 0)
      opt->mode = TEST_MODE;
#endif
    // Other options
    else if (strcmp(arg, "--keywords-anywhere") == 0 || strcmp(arg, "-k") == 0)
      opt->keywords_anywhere = true;
    else if (strcmp(arg, "--quiet") == 0 || strcmp(arg, "-q") == 0)
      opt->quiet = true;
    else if (strcmp(arg, "--randomize") == 0 || strcmp(arg, "-z") == 0)
      srand((unsigned)time(NULL));
    else if (strcmp(arg, "--report-memory") == 0 || strcmp(arg, "-m") == 0)
      opt->report_memory = true;
#if HAS_TIMER
    else if (strcmp(arg, "--time") == 0 || strcmp(arg, "-i") == 0)
      opt->report_time = true;
#endif
    else if (strcmp(arg, "--trace-basic") == 0 || strcmp(arg, "-t") == 0)
      opt->trace_basic = true;
    else if (strcmp(arg, "--trace-for") == 0 || strcmp(arg, "-f") == 0)
      opt->trace_for = true;
    else if (strcmp(arg, "--trace-log") == 0 || strcmp(arg, "-g") == 0)
      opt->trace_log = true;
    else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0)
      opt->print_version = true;
    else if (arg[0] == '-')
      fatal("unrecognised option: %s\n", arg);
    else if (opt->file_name == NULL)
      opt->file_name = arg;
    else
      fatal("unexpected argument: %s\n", arg);
  }
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

  puts("--trace-log, -g");
  if (full)
    puts("    Print a detailed log of program execution to stderr.\n"
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
