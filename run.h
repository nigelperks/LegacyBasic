// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#pragma once

#include <stdbool.h>
#include "bcode.h"
#include "paren.h"
#include "stringlist.h"

struct numvar {
  unsigned name;
  double val;
};

#define MAX_NUM_VAR (64)

typedef struct {
  struct numvar vars[MAX_NUM_VAR];
  unsigned count;
} NUMVARS;

struct strvar {
  unsigned name;
  char* val;
};

#define MAX_STR_VAR (32)

typedef struct {
  struct strvar vars[MAX_STR_VAR];
  unsigned count;
} STRVARS;

// Run-time environment of named symbols.
typedef struct {
  STRINGLIST* names;
  NUMVARS numvars;
  STRVARS strvars;
  PAREN_SYMBOLS paren;
} ENV;

ENV* new_environment(void);
void delete_environment(ENV*);
void clear_code_dependent_environment(ENV*);
void clear_environment(ENV*);

void run(BCODE*, ENV*, const SOURCE*, bool trace_basic, bool trace_for, bool randomize);

void trap_interrupt(void);
void untrap_interrupt(void);
