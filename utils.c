// Legacy BASIC
// Utility types and functions.
// Copyright (c) 2021-2 Nigel Perks

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include "utils.h"

const char* progname;

void fatal(const char* fmt, ...) {
  fflush(stdout);
  va_list ap;
  if (progname)
    fprintf(stderr, "%s: ", progname);
  fprintf(stderr, "fatal: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

void error(const char* fmt, ...) {
  fflush(stdout);
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  putchar('\n');
}

unsigned long malloc_count;
unsigned long free_count;

void* emalloc(size_t sz) {
  void* p = malloc(sz);
  if (p == NULL)
    fatal("out of memory (emalloc)\n");
  malloc_count++;
  return p;
}

void* erealloc(void* p, size_t sz) {
  if (p)
    free_count++;
  p = realloc(p, sz);
  if (p == NULL)
    fatal("out of memory (erealloc)\n");
  malloc_count++;
  return p;
}

void* ecalloc(size_t count, size_t size) {
  void* p = calloc(count, size);
  if (p == NULL)
    fatal("out of memory (ecalloc)\n");
  malloc_count++;
  return p;
}

char* estrdup(const char* s) {
  char* t = NULL;
  if (s) {
    if (s[0]) {
      t = emalloc(strlen(s) + 1);
      strcpy(t, s);
    }
    else {
      t = emalloc(1);
      t[0] = '\0';
    }
  }
  return t;
}

void efree(void* p) {
  if (p) {
    free_count++;
    free(p);
  }
}

bool string_name(const char* name) {
  return name != NULL && name[0] && name[strlen(name) - 1] == '$';
}

void space(unsigned n, FILE* fp) {
  while (n--)
    putc(' ', fp);
}

#ifdef UNIT_TEST

#include "CuTest.h"

static void test_string_name(CuTest* tc) {
  CuAssertIntEquals(tc, false, string_name(NULL));
  CuAssertIntEquals(tc, false, string_name(""));
  CuAssertIntEquals(tc, true, string_name("$"));
  CuAssertIntEquals(tc, true, string_name("a$"));
  CuAssertIntEquals(tc, false, string_name("a"));
}

CuSuite* utils_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_string_name);
  return suite;
}

#endif
