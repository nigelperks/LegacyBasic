// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#pragma once

typedef struct {
  char* name;
  int type;
  char* args;
  int opcode;
} BUILTIN;

extern BUILTIN builtins[];

const BUILTIN* builtin(const char*);
const BUILTIN* builtin_number(unsigned index);
