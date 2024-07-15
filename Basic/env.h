// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#pragma once

#include "stringlist.h"
#include "paren.h"

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

ENV* new_environment_with_builtins(void);
void delete_environment(ENV*);

void clear_environment(ENV*);

int env_lookup_numvar(const ENV*, unsigned name);
int env_lookup_strvar(const ENV*, unsigned name);
