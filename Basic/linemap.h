// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks
// Map Basic line number to value.

#pragma once

#include <stdbool.h>

typedef struct line_map LINE_MAP;

LINE_MAP* new_line_map(unsigned lines);
void delete_line_map(LINE_MAP*);
bool insert_line_mapping(LINE_MAP*, unsigned basic_line, unsigned value);
bool lookup_line_mapping(const LINE_MAP*, unsigned basic_line, unsigned *value);
