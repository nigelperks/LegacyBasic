// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#pragma once

#include <stdbool.h>
#include "bcode.h"

typedef struct vm VM;

VM* new_vm(bool keywords_anywhere, bool trace_basic, bool trace_for, bool trace_log);
void delete_vm(VM*);

// Maintain a source program.
void vm_new_program(VM*);
void vm_delete_source_line(VM*, unsigned num);
void vm_enter_source_line(VM*, unsigned num, const char* text);
bool vm_save_source(VM*, const char* name);
bool vm_load_source(VM*, const char* name);

const SOURCE* vm_stored_source(vm);

// Compile and run code.
void run_program(VM*);
void run_immediate(VM*, const char* line);

bool vm_continue(VM*);

// Maintain an environment of variables and functions.
void vm_clear_names(VM*);  // go back to builtin names only
void vm_clear_values(VM*);  // clear values but keep names list so code remains valid
