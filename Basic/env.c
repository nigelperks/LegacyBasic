// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#include <assert.h>
#include "env.h"
#include "utils.h"

ENV* new_environment_with_builtins(void) {
  ENV* env = emalloc(sizeof *env);
  env->names = new_stringlist();
  env->numvars.count = 0;
  env->strvars.count = 0;
  init_paren_symbols(&env->paren);
  insert_builtins(&env->paren, env->names);
  return env;
}

void delete_environment(ENV* env) {
  if (env) {
    for (unsigned i = 0; i < env->strvars.count; i++)
      efree(env->strvars.vars[i].val);
    deinit_paren_symbols(&env->paren);
    delete_stringlist(env->names);
    efree(env);
  }
}

// delete all values and definitions but keep names
// so that code using name indexes remains valid
void clear_environment(ENV* env) {
  assert(env != NULL);

  env->numvars.count = 0;

  for (unsigned i = 0; i < env->strvars.count; i++)
    efree(env->strvars.vars[i].val);
  env->strvars.count = 0;

  deinit_paren_symbols(&env->paren);

  insert_builtins(&env->paren, env->names);
}
