// Legacy BASIC
// Copyright (c) 2022 Nigel Perks
// Generic keyword processing.

#pragma once

typedef struct {
  char* name;
  unsigned short len;
  int token;
} KEYWORD;

int keyword_token(const KEYWORD* keywords, const char* identifier, int default_token);
const KEYWORD* keyword_prefix(const KEYWORD* keywords, const char* string);
const char* keyword_name(const KEYWORD* keywords, int token);
