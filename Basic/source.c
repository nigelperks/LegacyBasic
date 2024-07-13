// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include "source.h"
#include "utils.h"

SOURCE* new_source(const char* name) {
  SOURCE* p = ecalloc(1, sizeof (SOURCE));
  p->name = name ? estrdup(name) : NULL;
  return p;
}

void clear_source(SOURCE* src) {
  assert(src != NULL);
  for (unsigned i = 0; i < src->used; i++)
    efree(src->lines[i].text);
  src->used = 0;
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
  return src->name;
}

unsigned source_lines(const SOURCE* src) {
  assert(src != NULL);
  return src->used;
}

static void check_line(const SOURCE* src, unsigned line) {
  assert(src != NULL);
  if (line >= src->used)
    fatal("internal error: source line number out of range: %u\n", line);
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
  if (source != NULL && line < source_lines(source))
    fprintf(fp, "%u %s", source_linenum(source, line), source_text(source, line));
}

static void source_error(const SOURCE* src, const char* fmt, ...) {
  assert(src != NULL);
  if (src->name)
    fprintf(stderr, "%s(%u): ", src->name, src->used + 1);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

static void ensure_space(SOURCE* src) {
  assert(src->used <= src->allocated);
  if (src->used == src->allocated) {
    src->allocated = src->allocated ? 2 * src->allocated : 128;
    src->lines = erealloc(src->lines, src->allocated * sizeof src->lines[0]);
  }
}

static void append(SOURCE* src, unsigned num, const char* text) {
  assert(src != NULL);
  assert(text != NULL);
  if (num == 0)
    source_error(src, "invalid line number: %u\n", num);
  unsigned latest = src->used ? src->lines[src->used - 1].num : 0;
  if (num <= latest)
    source_error(src, "line number is not in increasing order: %u\n", num);
  ensure_space(src);
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
    source_error(src, "line has no line number\n");
  // treat one space as pure delimiter but preserve any other indenting
  if (*s == ' ')
    s++;
  return s;
}

static void spread(SOURCE* src, unsigned pos) {
  assert(src != NULL);
  assert(pos < src->used);
  ensure_space(src);
  assert(src->used < src->allocated);
  src->used++;
  for (unsigned i = src->used - 1; i > pos; i--)
    src->lines[i] = src->lines[i-1];
  src->lines[pos].num = 0;
  src->lines[pos].text = NULL;
}

void enter_source_line(SOURCE* src, unsigned num, const char* text) {
  assert(text != NULL);
  for (unsigned i = 0; i < src->used; i++) {
    if (src->lines[i].num == num) {
      efree(src->lines[i].text);
      src->lines[i].text = estrdup(text);
      return;
    }
    if (src->lines[i].num > num) {
      spread(src, i);
      src->lines[i].num = num;
      src->lines[i].text = estrdup(text);
      return;
    }
  }
  append(src, num, text);
}

void delete_source_line(SOURCE* src, unsigned line) {
  assert(src != NULL);
  if (line < src->used) {
    efree(src->lines[line].text);
    src->used--;
    for (unsigned i = line; i < src->used; i++)
      src->lines[i] = src->lines[i+1];
  }
}

SOURCE* load_source_file(const char* name) {
  FILE* fp = fopen(name, "r");
  if (fp == NULL) {
    fprintf(stderr, "Cannot open source file: %s\n", name);
    return NULL;
  }
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
    if (text == NULL)
      exit(EXIT_FAILURE);
    append(src, num, text);
  }
  return src;
}

void save_source_file(const SOURCE* src, const char* name) {
  FILE* fp = fopen(name, "w");
  if (fp == NULL) {
    source_error(src, "cannot create file: %s", name);
    return;
  }
  for (unsigned i = 0; i < src->used; i++)
    fprintf(fp, "%u %s\n", src->lines[i].num, src->lines[i].text);
  fclose(fp);
}

static unsigned line_length(const char* s) {
  assert(s != NULL);

  unsigned i;

  for (i = 0; s[i] && s[i] != '\n'; i++)
    ;

  return i;
}

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

SOURCE* load_source_string(const char* string, const char* name) {
  assert(string != NULL);
  assert(name != NULL);

  SOURCE* src = new_source(name);

  for (;;) {
    char buf[256];
    unsigned len = get_line(src, string, buf, sizeof buf);

    unsigned num;
    const char* text = parse_line_number(src, buf, &num);
    if (text == NULL)
      exit(EXIT_FAILURE);
    append(src, num, text);

    if (string[len] == '\0' || string[len+1] == '\0')
      break;

    string += len + 1;
  }

  return src;
}

SOURCE* wrap_source_text(const char* text) {
  assert(text != NULL);

  SOURCE* src = new_source(NULL);

  src->lines = emalloc(sizeof src->lines[0]);
  src->lines[0].num = 0;
  src->lines[0].text = estrdup(text);
  src->allocated = 1;
  src->used = 1;

  return src;
}

bool find_source_linenum(const SOURCE* source, unsigned num, unsigned *index) {
  assert(source != NULL);
  for (unsigned i = 0; i < source->used; i++) {
    if (source->lines[i].num == num) {
      *index = i;
      return true;
    }
  }
  return false;
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

static void test_clear(CuTest* tc) {
  SOURCE* p = new_source(NULL);
  p->lines = emalloc(4 * sizeof p->lines[0]);
  p->allocated = 4;
  p->lines[0].num = 100;
  p->lines[0].text = estrdup("hello");
  p->lines[1].num = 110;
  p->lines[1].text = estrdup("hello again");
  p->used = 2;
  CuAssertStrEquals(tc, NULL, source_name(p));
  CuAssertIntEquals(tc, 2, source_lines(p));
  clear_source(p);
  CuAssertIntEquals(tc, 0, p->used);
  CuAssertIntEquals(tc, 4, p->allocated);
  efree(p->lines);
  efree(p);
}

static void test_ensure_space(CuTest* tc) {
  SOURCE* p = new_source(NULL);

  ensure_space(p);
  CuAssertPtrNotNull(tc, p->lines);
  CuAssertIntEquals(tc, 128, p->allocated);
  p->lines[127].num = 10;
  p->used = 128;

  ensure_space(p);
  CuAssertPtrNotNull(tc, p->lines);
  CuAssertIntEquals(tc, 256, p->allocated);
  CuAssertIntEquals(tc, 10, p->lines[127].num);
  p->lines[255].num = 20;

  efree(p->lines);
  efree(p);
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

static void test_parse_line_number(CuTest* tc) {
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

static void test_spread(CuTest* tc) {
  SOURCE* p = new_source(NULL);

  append(p, 100, "PRINT");
  spread(p, 0);
  CuAssertIntEquals(tc, 2, p->used);
  CuAssertIntEquals(tc, 0, p->lines[0].num);
  CuAssertPtrEquals(tc, NULL, p->lines[0].text);
  CuAssertIntEquals(tc, 100, p->lines[1].num);
  CuAssertStrEquals(tc, "PRINT", p->lines[1].text);

  p->lines[0].num = 50;
  p->lines[0].text = estrdup("REM TEST");
  spread(p, 1);
  CuAssertIntEquals(tc, 3, p->used);
  CuAssertIntEquals(tc, 50, p->lines[0].num);
  CuAssertStrEquals(tc, "REM TEST", p->lines[0].text);
  CuAssertIntEquals(tc, 0, p->lines[1].num);
  CuAssertPtrEquals(tc, NULL, p->lines[1].text);
  CuAssertIntEquals(tc, 100, p->lines[2].num);
  CuAssertStrEquals(tc, "PRINT", p->lines[2].text);

  delete_source(p);
}

static void test_enter_find_delete(CuTest* tc) {
  SOURCE* p = new_source(NULL);

  enter_source_line(p, 100, "PRINT");
  CuAssertIntEquals(tc, 1, p->used);
  CuAssertIntEquals(tc, 100, p->lines[0].num);
  CuAssertStrEquals(tc, "PRINT", p->lines[0].text);

  enter_source_line(p, 200, "NEXT");
  CuAssertIntEquals(tc, 2, p->used);
  CuAssertIntEquals(tc, 200, p->lines[1].num);
  CuAssertStrEquals(tc, "NEXT", p->lines[1].text);

  enter_source_line(p, 150, "FOR");
  CuAssertIntEquals(tc, 3, p->used);
  CuAssertIntEquals(tc, 150, p->lines[1].num);
  CuAssertStrEquals(tc, "FOR", p->lines[1].text);
  CuAssertIntEquals(tc, 200, p->lines[2].num);
  CuAssertStrEquals(tc, "NEXT", p->lines[2].text);

  enter_source_line(p, 200, "GOSUB 2000");
  CuAssertIntEquals(tc, 3, p->used);
  CuAssertIntEquals(tc, 150, p->lines[1].num);
  CuAssertStrEquals(tc, "FOR", p->lines[1].text);
  CuAssertIntEquals(tc, 200, p->lines[2].num);
  CuAssertStrEquals(tc, "GOSUB 2000", p->lines[2].text);

  unsigned i;
  CuAssertIntEquals(tc, false, find_source_linenum(p, 10, &i));
  CuAssertIntEquals(tc, true, find_source_linenum(p, 100, &i));
  CuAssertIntEquals(tc, 0, i);
  CuAssertIntEquals(tc, false, find_source_linenum(p, 130, &i));
  CuAssertIntEquals(tc, true, find_source_linenum(p, 150, &i));
  CuAssertIntEquals(tc, 1, i);
  CuAssertIntEquals(tc, true, find_source_linenum(p, 200, &i));
  CuAssertIntEquals(tc, 2, i);

  CuAssertIntEquals(tc, 3, p->used);
  delete_source_line(p, 3);
  CuAssertIntEquals(tc, 3, p->used);

  delete_source_line(p, 1);
  CuAssertIntEquals(tc, 2, p->used);
  CuAssertIntEquals(tc, 100, p->lines[0].num);
  CuAssertIntEquals(tc, 200, p->lines[1].num);

  delete_source_line(p, 1);
  CuAssertIntEquals(tc, 1, p->used);
  CuAssertIntEquals(tc, 100, p->lines[0].num);

  delete_source_line(p, 0);
  CuAssertIntEquals(tc, 0, p->used);

  efree(p->lines);
  efree(p);
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

static void test_wrap(CuTest* tc) {
  SOURCE* p = wrap_source_text("immediate mode");
  CuAssertPtrNotNull(tc, p);
  CuAssertPtrEquals(tc, NULL, p->name);
  CuAssertPtrNotNull(tc, p->lines);
  CuAssertIntEquals(tc, 1, p->used);
  CuAssertIntEquals(tc, 0, p->lines[0].num);
  CuAssertStrEquals(tc, "immediate mode", p->lines[0].text);
  delete_source(p);
}

CuSuite* source_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_new);
  SUITE_ADD_TEST(suite, test_clear);
  SUITE_ADD_TEST(suite, test_ensure_space);
  SUITE_ADD_TEST(suite, test_append);
  SUITE_ADD_TEST(suite, test_parse_line_number);
  SUITE_ADD_TEST(suite, test_spread);
  SUITE_ADD_TEST(suite, test_enter_find_delete);
  SUITE_ADD_TEST(suite, test_line_length);
  SUITE_ADD_TEST(suite, test_get_line);
  SUITE_ADD_TEST(suite, test_load_string);
  SUITE_ADD_TEST(suite, test_wrap);
  return suite;
}

#endif
