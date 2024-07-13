// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks
// Utility functions for handling parenthesised symbols (array, DEF, builtin):
// A$(), FNA(), CHR$()

#pragma once

#include "arrays.h"
#include "def.h"
#include "stringlist.h"

enum { PK_ARRAY, PK_DEF, PK_BUILTIN };

const char* paren_kind(int PK);

typedef struct {
  unsigned name;
  short type;
  short kind;
  union {
    struct numeric_array * numarr;
    struct string_array * strarr;
    struct def * def;
  } u;
} PAREN_SYMBOL;

bool replace_numeric_array(PAREN_SYMBOL*, unsigned base, unsigned dimensions, unsigned max[]);
bool replace_string_array(PAREN_SYMBOL*, unsigned base, unsigned dimensions, unsigned max[]);
bool replace_def(PAREN_SYMBOL*, unsigned params, unsigned source_line, const BCODE*, unsigned pc);

typedef struct {
  PAREN_SYMBOL* sym;
  unsigned allocated;
  unsigned used;
} PAREN_SYMBOLS;

void init_paren_symbols(PAREN_SYMBOLS*);
void deinit_paren_symbols(PAREN_SYMBOLS*);

PAREN_SYMBOL* lookup_paren_name(const PAREN_SYMBOLS*, unsigned name);

void insert_builtins(PAREN_SYMBOLS*, STRINGLIST* names);

PAREN_SYMBOL* insert_numeric_array(PAREN_SYMBOLS*, unsigned name, unsigned base, unsigned dimensions, unsigned max[]);
PAREN_SYMBOL* insert_string_array(PAREN_SYMBOLS*, unsigned name, unsigned base, unsigned dimensions, unsigned max[]);
PAREN_SYMBOL* insert_def(PAREN_SYMBOLS*, unsigned name, int type, unsigned params, unsigned source_line, const BCODE*, unsigned pc);
