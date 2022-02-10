// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#pragma once

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

SOURCE* load_source_file(const char* file_name);
SOURCE* load_source_string(const char* string, const char* name);
void delete_source(SOURCE*);

const char* source_name(const SOURCE*);
unsigned source_lines(const SOURCE*);
unsigned source_linenum(const SOURCE*, unsigned line);
const char* source_text(const SOURCE*, unsigned line);
void print_source_line(const SOURCE*, unsigned line, FILE*);