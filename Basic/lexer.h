// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#pragma once

#include <stdbool.h>
#include "source.h"

#define MAX_WORD (128)

typedef struct {
  const char* name;
  unsigned lineno;
  const char* text;
  unsigned pos;
  unsigned token_pos;
  int token;
  double num;
  bool recognise_keyword_prefixes;
  char word[MAX_WORD];
} LEX;

LEX* new_lex(const char* name, bool recognise_keyword_prefixes);
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
