// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks
// Utility functions for DEF.

#pragma once

#include "source.h"
#include "bcode.h"

struct def {
  unsigned params;
  unsigned source_line;  // 0 if immediate statement
  BCODE* code;
};

struct def * new_def(void);
void delete_def(struct def *);
