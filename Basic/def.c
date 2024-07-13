// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks
// Utility functions for DEF.

#include "def.h"
#include "utils.h"

struct def * new_def(void) {
  struct def * p = emalloc(sizeof *p);
  p->params = 0;
  p->source_line = 0;
  p->code = NULL;
  return p;
}

void delete_def(struct def * def) {
  delete_bcode(def->code);
  efree(def);
}

