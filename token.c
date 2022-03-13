// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "token.h"
#include "os.h"

static const KEYWORD keywords[] = {
  { "AND", 3, TOK_AND },
  { "DATA", 4, TOK_DATA },
  { "DEF", 3, TOK_DEF },
  { "DIM", 3, TOK_DIM },
  { "END", 3, TOK_END },
  { "FOR", 3, TOK_FOR },
  { "GOSUB", 5, TOK_GOSUB },
  { "GOTO", 4, TOK_GOTO },
  { "IF", 2, TOK_IF },
  { "INPUT", 5, TOK_INPUT },
  { "LET", 3, TOK_LET },
  { "NEXT", 4, TOK_NEXT },
  { "LINE", 4, TOK_LINE },
  { "NOT", 3, TOK_NOT },
  { "ON", 2, TOK_ON },
  { "OR", 2, TOK_OR },
  { "PRINT", 5, TOK_PRINT },
  { "READ", 4, TOK_READ },
  { "REM", 3, TOK_REM },
  { "RESTORE", 7, TOK_RESTORE },
  { "RETURN", 6, TOK_RETURN },
  { "STEP", 4, TOK_STEP },
  { "STOP", 4, TOK_STOP },
  { "THEN", 4, TOK_THEN },
  { "TO", 2, TOK_TO },
  // end
  { NULL, 0, TOK_NONE }
};

int identifier_token(const char* s) {
  assert(s != NULL);

  for (const KEYWORD* p = keywords; p->name; p++) {
    if (__STRICMP(p->name, s) == 0)
      return p->token;
  }
  return TOK_ID;
}

const KEYWORD* keyword_prefix(const char* s) {
  assert(s != NULL);

  for (const KEYWORD* p = keywords; p->name; p++) {
    if (__STRNICMP(p->name, s, p->len) == 0)
      return p;
  }
  return NULL;
}

const char* token_name(int t) {
  for (const KEYWORD* p = keywords; p->name; p++) {
    if (p->token == t)
      return p->name;
  }

  const char* s = NULL;
  switch (t) {
    case '\n': s = "end of line"; break;
    case TOK_NONE: s = "no token"; break;
    case TOK_EOF: s = "end of file"; break;
    case TOK_ID: s = "name"; break;
    case TOK_NUM: s = "number"; break;
    case TOK_STR: s = "string"; break;
    case TOK_NE: s = "<>"; break;
    case TOK_LE: s = "<="; break;
    case TOK_GE: s = ">="; break;
  }
  return s;
}

void print_token(int token, FILE* fp) {
  const char* name = token_name(token);
  if (name) {
    fputs(name, fp);
    return;
  }

  if (token > 31 && token < 127) {
    fprintf(fp, "'%c'", token);
    return;
  }

  fprintf(fp, "unknown token: %d", token);
}


#ifdef UNIT_TEST

#include "CuTest.h"

static void test_identifier_token(CuTest* tc) {
  CuAssertIntEquals(tc, TOK_ID, identifier_token("X"));
  CuAssertIntEquals(tc, TOK_ID, identifier_token("<>"));
  CuAssertIntEquals(tc, TOK_AND, identifier_token("AND"));
  CuAssertIntEquals(tc, TOK_FOR, identifier_token("FOR"));
  CuAssertIntEquals(tc, TOK_TO, identifier_token("TO"));
}

static void test_keyword_prefix(CuTest* tc) {
  const KEYWORD * k;

  k = keyword_prefix("");
  CuAssertTrue(tc, k == NULL);

  k = keyword_prefix("AN");
  CuAssertTrue(tc, k == NULL);

  k = keyword_prefix("ANA");
  CuAssertTrue(tc, k == NULL);

  k = keyword_prefix("AND");
  CuAssertPtrNotNull(tc, k);
  CuAssertIntEquals(tc, TOK_AND, k->token);

  k = keyword_prefix("FOR");
  CuAssertPtrNotNull(tc, k);
  CuAssertIntEquals(tc, TOK_FOR, k->token);

  k = keyword_prefix("FORM");
  CuAssertPtrNotNull(tc, k);
  CuAssertIntEquals(tc, TOK_FOR, k->token);
  CuAssertIntEquals(tc, 3, k->len);
}

static void test_token_name(CuTest* tc) {
  CuAssertTrue(tc, token_name(0) == NULL);
  CuAssertTrue(tc, token_name('A') == NULL);
  CuAssertStrEquals(tc, "<>", token_name(TOK_NE));
  CuAssertStrEquals(tc, "RESTORE", token_name(TOK_RESTORE));
}

CuSuite* token_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_identifier_token);
  SUITE_ADD_TEST(suite, test_keyword_prefix);
  SUITE_ADD_TEST(suite, test_token_name);
  return suite;
}

#endif