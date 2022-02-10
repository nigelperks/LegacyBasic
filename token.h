// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#pragma once

enum {
  TOK_NONE = 256,
  TOK_EOF,
  TOK_ID,
  TOK_NUM,
  TOK_STR,
  // operators
  TOK_NE,
  TOK_LE,
  TOK_GE,
  // keywords
  TOK_AND,
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

typedef struct {
  char* name;
  unsigned short len;
  int token;
} KEYWORD;

const KEYWORD* keyword_prefix(const char*);