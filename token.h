// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#pragma once

#include "keyword.h"

enum {
  TOK_NONE = 256,
  TOK_ERROR,
  TOK_EOF,
  TOK_ID,
  TOK_NUM,
  TOK_STR,
  // operators
  TOK_NE,
  TOK_LE,
  TOK_GE,
  // end of non-keyword tokens
  FIRST_KEYWORD_TOKEN
};

enum {
  TOK_AND = FIRST_KEYWORD_TOKEN,
  TOK_CLEAR,
  TOK_DATA,
  TOK_DEF,
  TOK_DIM,
  TOK_END,
  TOK_FOR,
  TOK_GOSUB,
  TOK_GOTO,
  TOK_IF,
  TOK_INPUT,
  TOK_LET,
  TOK_LINE,
  TOK_NEXT,
  TOK_NOT,
  TOK_ON,
  TOK_OR,
  TOK_PRINT,
  TOK_READ,
  TOK_REM,
  TOK_RESTORE,
  TOK_RETURN,
  TOK_STEP,
  TOK_STOP,
  TOK_THEN,
  TOK_TO,
};

const char* token_name(int token);

int identifier_token(const char*);

void print_token(int token, FILE*);

const KEYWORD* token_prefix(const char*);

extern const KEYWORD basic_keywords[];
