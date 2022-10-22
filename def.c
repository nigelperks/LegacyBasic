// Legacy BASIC
// Copyright (c) 2022 Nigel Perks
// Utility functions for DEF.

#include "def.h"
#include "utils.h"

struct def * new_def(unsigned params, unsigned source_line, unsigned pc) {
  struct def * p = emalloc(sizeof *p);
  p->params = params;
  p->source_line = source_line;
  p->pc = pc;
  return p;
}

void delete_def(struct def * def) {
  efree(def);
}

