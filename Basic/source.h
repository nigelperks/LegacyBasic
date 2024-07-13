// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#pragma once

#include <stdio.h>
#include <stdbool.h>

struct source_line {
  unsigned num;
  char* text;
};

typedef struct {
  char* name;
  struct source_line * lines;
  unsigned allocated;
  unsigned used;
} SOURCE;

SOURCE* new_source(const char* name);
void delete_source(SOURCE*);

void clear_source(SOURCE*);

bool find_source_linenum(const SOURCE*, unsigned num, unsigned *index);
void enter_source_line(SOURCE*, unsigned num, const char* text);
void delete_source_line(SOURCE*, unsigned index);

SOURCE* load_source_file(const char* file_name);
SOURCE* load_source_string(const char* string, const char* name);
SOURCE* wrap_source_text(const char* text);
void save_source_file(const SOURCE*, const char* name);

const char* source_name(const SOURCE*);
unsigned source_lines(const SOURCE*);
unsigned source_linenum(const SOURCE*, unsigned line);
const char* source_text(const SOURCE*, unsigned line);
void print_source_line(const SOURCE*, unsigned line, FILE*);
