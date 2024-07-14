// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#pragma once

// B-code intermediate code

#include <stdio.h>
#include <stdbool.h>
#include "source.h"
#include "stringlist.h"

enum {
  // placeholder
  B_NOP,
  // source
  B_SOURCE_LINE,
  // whole environment
  B_CLEAR,
  // numbers
  B_PUSH_NUM,
  B_POP_NUM,
  B_GET_SIMPLE_NUM,
  B_SET_SIMPLE_NUM,
  B_DIM_NUM,
  B_GET_PAREN_NUM,
  B_SET_ARRAY_NUM,
  B_ADD,
  B_SUB,
  B_MUL,
  B_DIV,
  B_POW,
  B_EQ_NUM,
  B_LT_NUM,
  B_GT_NUM,
  B_NE_NUM,
  B_LE_NUM,
  B_GE_NUM,
  B_OR,
  B_AND,
  B_NOT,
  B_NEG,
  // strings
  B_PUSH_STR,
  B_POP_STR,
  B_SET_SIMPLE_STR,
  B_GET_SIMPLE_STR,
  B_DIM_STR,
  B_GET_PAREN_STR,
  B_SET_ARRAY_STR,
  B_EQ_STR,
  B_NE_STR,
  B_LT_STR,
  B_GT_STR,
  B_LE_STR,
  B_GE_STR,
  B_CONCAT,
  // control flow
  B_END,
  B_STOP,
  B_GOTO,
  B_GOTRUE,
  B_GOSUB,
  B_RETURN,
  B_FOR,
  B_NEXT_VAR,
  B_NEXT_IMP,
  B_DEF,
  B_PARAM,
  B_END_DEF,
  B_ON_GOTO,
  B_ON_GOSUB,
  B_ON_LINE,
  B_IF_THEN,
  B_IF_ELSE,
  B_ELSE,
  // output
  B_PRINT_LN,
  B_PRINT_SPC,
  B_PRINT_TAB,
  B_PRINT_COMMA,
  B_PRINT_NUM,
  B_PRINT_STR,
  B_CLS,
  // input
  B_INPUT_BUF,
  B_INPUT_END,
  B_INPUT_SEP,
  B_INPUT_NUM,
  B_INPUT_STR,
  B_INPUT_LINE,
  // inline data
  B_DATA,
  B_READ_NUM,
  B_READ_STR,
  B_RESTORE,
  B_RESTORE_LINE,
  // random numbers
  B_RAND,
  B_SEED,
  // builtins
  B_ABS,
  B_ASC,
  B_ATN,
  B_CHR,
  B_COS,
  B_EXP,
  B_INKEY,
  B_INT,
  B_LEFT,
  B_LEN,
  B_LOG,
  B_MID3,
  B_RIGHT,
  B_RND,
  B_SGN,
  B_SIN,
  B_SQR,
  B_STR,
  B_TAN,
  B_TIME_STR,
  B_VAL,
};

enum bcode_format {
  BF_IMPLICIT,
  BF_SOURCE_LINE,
  BF_BASIC_LINE,
  BF_NUM,
  BF_STR,
  BF_VAR,
  BF_PARAM,
  BF_COUNT,
};

typedef struct {
  unsigned short op;
  union {
    unsigned source_line;
    unsigned basic_line;
    double num;
    char* str;
    unsigned name;
    struct {
      unsigned name;
      unsigned char params;
    } param;
    unsigned count;
  } u;
} BINST;

typedef struct {
  BINST* inst;
  unsigned allocated;
  unsigned used;
} BCODE;

BCODE* new_bcode(void);
void delete_bcode(BCODE*);
const BINST* bcode_latest(const BCODE*);
BINST* bcode_next(BCODE*, unsigned op);

void print_bcode(const BCODE*, const SOURCE*, const STRINGLIST* names, FILE*);
void print_binst(const BCODE*, unsigned index, const SOURCE*, const STRINGLIST* names, FILE*);

BCODE* bcode_copy_def(const BCODE*, unsigned start);

typedef struct bcode_index BCODE_INDEX;

BCODE_INDEX* bcode_index(const BCODE*, const SOURCE*);
void delete_bcode_index(BCODE_INDEX*);
bool bcode_find_indexed_basic_line(const BCODE_INDEX*, unsigned basic_line, unsigned *bcode_pos);
