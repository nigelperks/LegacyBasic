// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks
// Utility functions for DEF.

#pragma once

#include "source.h"
#include "bcode.h"

struct def {
  BCODE* bcode;
  SOURCE* source; // reference to stored program source if applicable
  unsigned source_line;
};

// takes ownership of the BCODE
struct def * new_def(BCODE*, SOURCE*, unsigned source_line);

void delete_def(struct def *);
