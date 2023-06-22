// Legacy BASIC
// Copyright (c) 2022-3 Nigel Perks

#pragma once

#include "bcode.h"

void emit(BCODE*, unsigned op);
void emit_line(BCODE*, unsigned op, unsigned line);
void emit_num(BCODE*, unsigned op, double num);
void emit_str(BCODE*, unsigned op, const char* str);
void emit_str_ptr(BCODE*, unsigned op, char* str);
void emit_var(BCODE*, unsigned op, unsigned name);
unsigned emit_param(BCODE*, unsigned op, unsigned name, unsigned parameters);
unsigned emit_count(BCODE*, unsigned op, unsigned count);

void patch_count(BCODE*, unsigned index, unsigned count);
