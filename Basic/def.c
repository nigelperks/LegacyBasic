// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks
// Utility functions for DEF.

#include "def.h"
#include "utils.h"

struct def * new_def(BCODE* bc, SOURCE* source, unsigned source_line) {
  struct def * def = emalloc(sizeof *def);
  def->bcode = bc;
  def->source = source;
  def->source_line = source_line;
  return def;
}

void delete_def(struct def * def) {
  if (def) {
    delete_bcode(def->bcode);
    efree(def);
  }
}
