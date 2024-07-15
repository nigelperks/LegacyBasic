// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks
// Utility functions for BASIC arrays.

#pragma once

#include <stdbool.h>

#define MAX_DIMENSIONS (2)

#define MAX_ELEMENTS (64 * 1024)

struct array_size {
  unsigned short base;
  unsigned short dimensions;
  unsigned short max[MAX_DIMENSIONS];
  unsigned elements;
};

struct numeric_array {
  struct array_size size;
  double val[0];
};

struct numeric_array * new_numeric_array(unsigned base, unsigned dimensions, unsigned max[]);
void delete_numeric_array(struct numeric_array *);
bool compute_numeric_element(struct numeric_array *, unsigned dimensions, const unsigned indexes[], double* *addr);

struct string_array {
  struct array_size size;
  char* val[0];
};

struct string_array * new_string_array(unsigned base, unsigned dimensions, unsigned max[]);
void delete_string_array(struct string_array *);
bool compute_string_element(struct string_array *, unsigned dimensions, const unsigned indexes[], char* * *addr);
