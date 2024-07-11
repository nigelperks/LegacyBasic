// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#pragma once

#include <stdbool.h>
#include "keyword.h"
#include "source.h"

typedef struct {
  const char* name;
  const KEYWORD* keywords;
  unsigned lineno;
  const char* text;
  unsigned pos;
  unsigned token_pos;
  int token;
  double num;
  bool recognise_keyword_prefixes;
  char word[128];
} LEX;

LEX* new_lex(const char* name, const KEYWORD* keywords, bool recognise_keyword_prefixes);
void delete_lex(LEX*);

int lex_line(LEX*, unsigned lineno, const char* text);

int lex_peek(LEX*);

unsigned lex_line_num(LEX*);
const char* lex_line_text(LEX*);
const char* lex_remaining(LEX*);
bool lex_unsigned(LEX*, unsigned *val);

int lex_next(LEX*);
int lex_token(LEX*);
const char* lex_word(LEX*);
double lex_num(LEX*);

const char* lex_next_data(LEX*);
void lex_discard(LEX*);

unsigned lex_token_pos(LEX*);

void print_lex_token(LEX*, FILE*);