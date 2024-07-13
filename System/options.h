// Legacy BASIC
// Basic interpreter for running 70s/80s microcomputer Basic games.
// Copyright (c) 2022-24 Nigel Perks
// Command line options.

#pragma once

#include <stdbool.h>

enum mode { NO_MODE, LIST_MODE, LIST_NAMES_MODE, PARSE_MODE, CODE_MODE, RUN_MODE, TEST_MODE };

typedef struct {
  int mode;
  const char* file_name;
  bool keywords_anywhere;
  bool print_version;
  bool quiet;
  bool report_memory;
  bool report_time;
  bool trace_basic;
  bool trace_for;
  bool trace_log;
} Options;

void init_options(Options*);
void parse_options(Options*, const char* argv[]);
