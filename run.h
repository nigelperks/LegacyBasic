// Legacy BASIC
// Copyright (c) 2022-3 Nigel Perks

#pragma once

#include <stdbool.h>
#include "bcode.h"
#include "env.h"

typedef struct vm VM;

VM* new_vm(bool keywords_anywhere, bool trace_basic, bool trace_for, bool trace_log);
void delete_vm(VM*);

// Maintain a source program.
void vm_new_program(VM*);
void vm_delete_source_line(VM*, unsigned num);
void vm_enter_source_line(VM*, unsigned num, const char* text);
void vm_take_source(VM*, SOURCE*);
unsigned vm_source_lines(const VM*);
unsigned vm_source_linenum(const VM*, unsigned index);
const char* vm_source_text(const VM*, unsigned index);

// Compile and run program.
void run_program(VM*);
void run_immediate(VM*, const SOURCE*, bool keywords_anywhere);
void vm_print_bcode(VM*);

// Maintain an environment of variables and functions.
void vm_new_environment(VM*);  // go back to builtin names only
void vm_clear_environment(VM*);  // clear values but keep names list so code remains valid
