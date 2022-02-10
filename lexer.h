// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#pragma once

#include <stdbool.h>
#include "source.h"

typedef struct {
  const SOURCE* source;
  unsigned line;
  const char* text;
  unsigned pos;
  unsigned token_pos;
  int token;
  double num;
  bool recognise_keyword_prefixes;
  char word[128];
} LEX;

LEX* new_lex(const SOURCE*, bool recognise_keyword_prefixes);
void delete_lex(LEX*);

void lex_line(LEX*, unsigned line);

unsigned lex_line_num(LEX*);
const char* lex_line_text(LEX*);

int lex_next(LEX*);
int lex_token(LEX*);
const char* lex_word(LEX*);
double lex_num(LEX*);

const char* lex_next_data(LEX*);
void lex_discard(LEX*);

unsigned lex_token_pos(LEX*);

void print_lex_token(LEX*, FILE*);