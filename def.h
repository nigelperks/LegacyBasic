#pragma once

struct def {
  unsigned params;
  unsigned source_line;
  unsigned pc;
};

struct def * new_def(unsigned params, unsigned source_line, unsigned pc);
void delete_def(struct def *);
