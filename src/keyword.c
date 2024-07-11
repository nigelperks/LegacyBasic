// Legacy BASIC
// Copyright (c) 2022 Nigel Perks
// Functions that work on any array of keywords whose last, terminator, entry has name == NULL.

#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "keyword.h"
#include "os.h"

int keyword_token(const KEYWORD* keywords, const char* s, int default_token) {
  assert(keywords != NULL && s != NULL);

  for (const KEYWORD* p = keywords; p->name; p++) {
    if (STRICMP(p->name, s) == 0)
      return p->token;
  }

  return default_token;
}

const KEYWORD* keyword_prefix(const KEYWORD* keywords, const char* s) {
  assert(keywords != NULL && s != NULL);

  for (const KEYWORD* p = keywords; p->name; p++) {
    if (STRNICMP(p->name, s, p->len) == 0)
      return p;
  }

  return NULL;
}

const char* keyword_name(const KEYWORD* keywords, int t) {
  for (const KEYWORD* p = keywords; p->name; p++) {
    if (p->token == t)
      return p->name;
  }

  return NULL;
}

#ifdef UNIT_TEST

#include "CuTest.h"

static const KEYWORD kw[] = {
  { "FRED", 4, 0 },
  { "Henry", 5, 1 },
  { NULL, 0, 0 }
};

static void test_keyword_token(CuTest* tc) {
  CuAssertIntEquals(tc, 0, keyword_token(kw, "FRED", -1));
  CuAssertIntEquals(tc, 0, keyword_token(kw, "Fred", -1));
  CuAssertIntEquals(tc, -1, keyword_token(kw, "Freddy", -1));
  CuAssertIntEquals(tc, 1, keyword_token(kw, "Henry", -1));
  CuAssertIntEquals(tc, 1, keyword_token(kw, "HENRY", -1));
  CuAssertIntEquals(tc, -1, keyword_token(kw, "HENR", -1));
}

static void test_keyword_prefix(CuTest* tc) {
  CuAssertTrue(tc, keyword_prefix(kw, "FRED") == &kw[0]);
  CuAssertTrue(tc, keyword_prefix(kw, "FREDERICK") == &kw[0]);
  CuAssertTrue(tc, keyword_prefix(kw, "FRE") == NULL);
  CuAssertTrue(tc, keyword_prefix(kw, "henryetta") == &kw[1]);
  CuAssertTrue(tc, keyword_prefix(kw, "") == NULL);
}

static void test_keyword_name(CuTest* tc) {
  CuAssertStrEquals(tc, "FRED", keyword_name(kw, 0));
  CuAssertStrEquals(tc, "Henry", keyword_name(kw, 1));
  CuAssertStrEquals(tc, NULL, keyword_name(kw, 2));
}

CuSuite* keyword_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_keyword_token);
  SUITE_ADD_TEST(suite, test_keyword_prefix);
  SUITE_ADD_TEST(suite, test_keyword_name);
  return suite;
}

#endif
