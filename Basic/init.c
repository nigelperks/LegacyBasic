#include "init.h"
#include "builtin.h"

void init_builtins(SYMTAB* st) {
  const BUILTIN* b;
  for (unsigned i = 0; b = builtin_number(i); i++) {
    SYMBOL* sym = sym_insert(st, b->name, SYM_BUILTIN, b->type);
    sym->val.builtin.args = b->args;
    sym->val.builtin.opcode = b->opcode;
    sym->defined = true;
  }
}
