// Legacy BASIC
// Copyright (c) 2024 Nigel Perks
// Symbol table used for both compiling and running.

#pragma once

#include <stdbool.h>
#include "arrays.h"
#include "def.h"

enum symbol_kind {
  SYM_UNKNOWN,  // parenthesised symbol used before defined
  SYM_VARIABLE, // simple variable
  SYM_ARRAY,    // array
  SYM_DEF,      // DEF user-defined function
  SYM_BUILTIN   // built-in function
};

const char* symbol_kind(int kind);

typedef unsigned short SYMID;

typedef struct {
  char* name;
  SYMID id;
  char kind;
  char type;
  char defined;
  union {
    double num;
    char* str;
    struct numeric_array * numarr;
    struct string_array * strarr;
    struct def * def;
    struct builtin {
      const char* args;
      short opcode;
    } builtin;
  } val;
} SYMBOL;

// I want a particular symbol's address (SYMBOL* value) to be unchanged.
// So any reallocatable structure contains SYMBOL*, not SYMBOL.
// Initially use linear array and linear search.
typedef struct {
  SYMBOL* *psym;
  unsigned allocated;
  unsigned used;
  SYMID next_id;
} SYMTAB;

SYMTAB* new_symbol_table(void);
void delete_symbol_table(SYMTAB*);
void clear_symbol_table_values(SYMTAB*); // but keep names so that bcode referencing them remains valid
void clear_symbol_table_names(SYMTAB*);

SYMBOL* sym_lookup(SYMTAB*, const char* name, bool paren);
SYMBOL* sym_insert(SYMTAB*, const char* name, int kind, int type);
SYMBOL* sym_insert_builtin(SYMTAB*, const char* name, int type, const char* args, int opcode);

SYMBOL* symbol(SYMTAB*, SYMID);
const char* sym_name(const SYMTAB*, SYMID);

// Turn parenthesised symbols of UNKNOWN kind into ARRAY.
void sym_make_unknown_array(SYMTAB*);

// in symbol.h because symbol.h depends on bcode.h
void print_binst(const BINST*, unsigned j, const SOURCE* source, const SYMTAB* st, FILE* fp);
