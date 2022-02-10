// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include "source.h"
#include "utils.h"

static SOURCE* new_source(const char* name) {
  assert(name != NULL);
  SOURCE* p = ecalloc(1, sizeof (SOURCE));
  p->name = estrdup(name);
  return p;
}

void delete_source(SOURCE* src) {
  if (src) {
    efree(src->name);
    for (unsigned i = 0; i < src->used; i++)
      efree(src->lines[i].text);
    efree(src->lines);
    efree(src);
  }
}

const char* source_name(const SOURCE* src) {
  assert(src != NULL);
  return src->name;
}

unsigned source_lines(const SOURCE* src) {
  assert(src != NULL);
  return src->used;
}

static void check_line(const SOURCE* src, unsigned line) {
  if (line >= src->used)
    fatal("source line number out of range: %u\n", line);
}

const char* source_text(const SOURCE* src, unsigned line) {
  assert(src != NULL);
  assert(line < src->used);
  check_line(src, line);
  return src->lines[line].text;
}

unsigned source_linenum(const SOURCE* src, unsigned line) {
  assert(src != NULL);
  check_line(src, line);
  return src->lines[line].num;
}

void print_source_line(const SOURCE* source, unsigned line, FILE* fp) {
  if (line < source_lines(source))
    fprintf(fp, "%u %s\n", source_linenum(source, line), source_text(source, line));
}

static void source_error(SOURCE* src, const char* fmt, ...) {
  assert(src != NULL);
  fprintf(stderr, "%s(%u): ", src->name, src->used + 1);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

static void append(SOURCE* src, unsigned num, const char* text) {
  assert(src != NULL);
  assert(text != NULL);
  if (num == 0)
    source_error(src, "invalid line number: %u\n", num);
  unsigned latest = src->used ? src->lines[src->used - 1].num : 0;
  if (num <= latest)
    source_error(src, "line number is not in increasing order: %u\n", num);
  assert(src->used <= src->allocated);
  if (src->used == src->allocated) {
    src->allocated = src->allocated ? 2 * src->allocated : 128;
    src->lines = erealloc(src->lines, src->allocated * sizeof src->lines[0]);
  }
  assert(src->used < src->allocated);
  src->lines[src->used].num = num;
  src->lines[src->used].text = estrdup(text);
  src->used++;
}

static const char* parse_line_number(SOURCE* src, const char* line, unsigned *num) {
  assert(line != NULL);
  assert(num != NULL);
  *num = 0;
  const char* s = line;
  for (; isdigit(*s); s++)
    *num = *num * 10 + *s - '0';
  if (s == line)
    source_error(src, "line has no line number");
  // treat one space as pure delimiter but preserve any other indenting
  if (*s == ' ')
    s++;
  return s;
}

SOURCE* load_source_file(const char* name) {
  FILE* fp = fopen(name, "r");
  if (fp == NULL)
    fatal("cannot open source file: %s", name);
  SOURCE* src = new_source(name);
  char buf[256];
  while (fgets(buf, sizeof buf, fp)) {
    size_t len = strlen(buf);
    if (len > 0) {
      if (buf[len-1] != '\n')
        source_error(src, "source line is too long");
      len--;
      buf[len] = '\0';
    }
    if (len == 0)
      source_error(src, "source line is empty");
    unsigned num;
    const char* text = parse_line_number(src, buf, &num);
    append(src, num, text);
  }
  return src;
}

static unsigned get_line(SOURCE* src, const char* string, char* buf, unsigned bufsz);

SOURCE* load_source_string(const char* string, const char* name) {
  assert(string != NULL);
  assert(name != NULL);

  SOURCE* src = new_source(name);

  for (;;) {
    char buf[256];
    unsigned len = get_line(src, string, buf, sizeof buf);

    unsigned num;
    const char* text = parse_line_number(src, buf, &num);
    append(src, num, text);

    if (string[len] == '\0' || string[len+1] == '\0')
      break;

    string += len + 1;
  }

  return src;
}

static unsigned line_length(const char*);

static unsigned get_line(SOURCE* src, const char* string, char* buf, unsigned bufsz) {
    unsigned len = line_length(string);
    if (len == 0)
      source_error(src, "source line is empty");
    if (len + 1 > bufsz)
      source_error(src, "source line is too long");
    strncpy(buf, string, len);
    buf[len] = '\0';
    return len;
}

static unsigned line_length(const char* s) {
  assert(s != NULL);

  unsigned i;

  for (i = 0; s[i] && s[i] != '\n'; i++)
    ;

  return i;
}

#ifdef UNIT_TEST

#include "CuTest.h"

static void test_new(CuTest* tc) {
  SOURCE* p;
  char name[] = "test";

  p = new_source(name);
  CuAssertPtrNotNull(tc, p);
  CuAssertStrEquals(tc, "test", p->name);
  CuAssertTrue(tc, p->name != name);
  CuAssertPtrEquals(tc, NULL, p->lines);
  CuAssertIntEquals(tc, 0, p->allocated);
  CuAssertIntEquals(tc, 0, p->used);

  CuAssertStrEquals(tc, "test", source_name(p));
  CuAssertIntEquals(tc, 0, source_lines(p));

  delete_source(p);
}

static void test_parse(CuTest* tc) {
  SOURCE* p = new_source("test");
  unsigned num;
  const char* text;

  text = parse_line_number(p, "10 PRINT  ", &num);
  CuAssertIntEquals(tc, 10, num);
  CuAssertPtrNotNull(tc, text);
  CuAssertStrEquals(tc, "PRINT  ", text);

  text = parse_line_number(p, "20   NEXT Z", &num);
  CuAssertIntEquals(tc, 20, num);
  CuAssertPtrNotNull(tc, text);
  CuAssertStrEquals(tc, "  NEXT Z", text);

  delete_source(p);
}

static void test_append(CuTest* tc) {
  SOURCE* p;

  p = new_source("test");

  append(p, 10, "PRINT PI");
  CuAssertPtrNotNull(tc, p->lines);
  CuAssertTrue(tc, p->allocated >= 1);
  CuAssertIntEquals(tc, 1, p->used);
  CuAssertIntEquals(tc, 10, p->lines[0].num);
  CuAssertStrEquals(tc, "PRINT PI", p->lines[0].text);

  CuAssertIntEquals(tc, 1, source_lines(p));
  CuAssertIntEquals(tc, 10, source_linenum(p, 0));
  CuAssertStrEquals(tc, "PRINT PI", source_text(p, 0));

  append(p, 20, "INPUT k$");
  CuAssertPtrNotNull(tc, p->lines);
  CuAssertTrue(tc, p->allocated >= 2);
  CuAssertIntEquals(tc, 2, p->used);
  CuAssertIntEquals(tc, 10, p->lines[0].num);
  CuAssertStrEquals(tc, "PRINT PI", p->lines[0].text);
  CuAssertIntEquals(tc, 20, p->lines[1].num);
  CuAssertStrEquals(tc, "INPUT k$", p->lines[1].text);

  CuAssertIntEquals(tc, 2, source_lines(p));
  CuAssertIntEquals(tc, 10, source_linenum(p, 0));
  CuAssertStrEquals(tc, "PRINT PI", source_text(p, 0));
  CuAssertIntEquals(tc, 20, source_linenum(p, 1));
  CuAssertStrEquals(tc, "INPUT k$", source_text(p, 1));

  delete_source(p);
}

static void test_line_length(CuTest* tc) {
  CuAssertIntEquals(tc, 0, line_length(""));
  CuAssertIntEquals(tc, 5, line_length("HELLO"));
  CuAssertIntEquals(tc, 3, line_length("the\nman\n"));
}

static void test_get_line(CuTest* tc) {
  SOURCE source;
  char buf[16];
  unsigned len;

  memset(&source, 0, sizeof source);

  len = get_line(&source, "null", buf, sizeof buf);
  CuAssertIntEquals(tc, 4, len);
  CuAssertStrEquals(tc, "null", buf);

  len = get_line(&source, "newline\nnext\n", buf, sizeof buf);
  CuAssertIntEquals(tc, 7, len);
  CuAssertStrEquals(tc, "newline", buf);
}

static void test_load_string(CuTest* tc) {
  static const char CODE[] =
      "10 rem nonsense\n"
      "20 print 3.14\n"
      ;

  SOURCE* src = load_source_string(CODE, "lemon");
  CuAssertPtrNotNull(tc, src);

  CuAssertStrEquals(tc, "lemon", src->name);
  CuAssertPtrNotNull(tc, src->lines);
  CuAssertIntEquals(tc, 2, src->used);

  CuAssertIntEquals(tc, 10, src->lines[0].num);
  CuAssertStrEquals(tc, "rem nonsense", src->lines[0].text);

  CuAssertIntEquals(tc, 20, src->lines[1].num);
  CuAssertStrEquals(tc, "print 3.14", src->lines[1].text);

  delete_source(src);
}

CuSuite* source_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_new);
  SUITE_ADD_TEST(suite, test_parse);
  SUITE_ADD_TEST(suite, test_append);
  SUITE_ADD_TEST(suite, test_line_length);
  SUITE_ADD_TEST(suite, test_get_line);
  SUITE_ADD_TEST(suite, test_load_string);
  return suite;
}

#endif
