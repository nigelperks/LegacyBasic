// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks
// Basic language tokens.

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "token.h"
#include "os.h"
#include "hash.h"
#include "utils.h"

const KEYWORD keywords[] = {
  { "AND", 3, TOK_AND },
  { "CLEAR", 5, TOK_CLEAR },
  { "CLS", 3, TOK_CLS },
  { "DATA", 4, TOK_DATA },
  { "DEF", 3, TOK_DEF },
  { "DIM", 3, TOK_DIM },
  { "ELSE", 4, TOK_ELSE },
  { "END", 3, TOK_END },
  { "FOR", 3, TOK_FOR },
  { "GOSUB", 5, TOK_GOSUB },
  { "GOTO", 4, TOK_GOTO },
  { "IF", 2, TOK_IF },
  { "INPUT", 5, TOK_INPUT },
  { "LET", 3, TOK_LET },
  { "LINE", 4, TOK_LINE },
  { "NEXT", 4, TOK_NEXT },
  { "NOT", 3, TOK_NOT },
  { "ON", 2, TOK_ON },
  { "OR", 2, TOK_OR },
  { "PRINT", 5, TOK_PRINT },
  { "RANDOMIZE", 9, TOK_RANDOMIZE },
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

#define KEYWORD_HASH_SIZE (79)

struct keyword_node {
  KEYWORD kw;
  struct keyword_node * next;
};

struct {
  struct keyword_node* kw[KEYWORD_HASH_SIZE];
} keyword_hash;

void init_keywords(void) {
  int token = keywords[0].token - 1;

  for (const KEYWORD* p = keywords; p->name; p++) {
    if (p->token != token + 1)
      fatal("internal error: keyword tokens not consecutive: %s\n", p->name);
    token++;

    unsigned h = hashpjw_upper(p->name) % KEYWORD_HASH_SIZE;
    struct keyword_node * node = emalloc(sizeof *node);
    node->kw = *p;
    node->next = keyword_hash.kw[h];
    keyword_hash.kw[h] = node;
  }

#if 0
  // report hash table statistics
  unsigned used_slots = 0;
  unsigned max_nodes = 0;
  for (unsigned h = 0; h < KEYWORD_HASH_SIZE; h++) {
    if (keyword_hash.kw[h]) {
      used_slots++;
      unsigned count = 0;
      for (struct keyword_node * n = keyword_hash.kw[h]; n; n = n->next)
        count++;
      if (count > max_nodes)
        max_nodes = count;
    }
  }
  printf("-- used slots %u, max nodes in a slot %u\n", used_slots, max_nodes);
#endif
}

void deinit_keywords(void) {
  for (unsigned h = 0; h < KEYWORD_HASH_SIZE; h++) {
    struct keyword_node * next;
    for (struct keyword_node * p = keyword_hash.kw[h]; p; p = next) {
      next = p->next;
      efree(p);
    }
  }
}

int identifier_token(const char* s) {
  unsigned h = hashpjw_upper(s) % KEYWORD_HASH_SIZE;

  for (const struct keyword_node * p = keyword_hash.kw[h]; p; p = p->next) {
    if (STRICMP(p->kw.name, s) == 0)
      return p->kw.token;
  }

  return TOK_ID;
}

const char* keyword_name(int t) {
  return (t >= TOK_AND && t <= TOK_TO) ? keywords[t - TOK_AND].name : NULL;
}

const KEYWORD* keyword_prefix(const char* s) {
  assert(keywords != NULL && s != NULL);

  for (const KEYWORD* p = keywords; p->name; p++) {
    if (STRNICMP(p->name, s, p->len) == 0)
      return p;
  }

  return NULL;
}

static const char* token_name(int t) {
  const char* s = NULL;
  switch (t) {
    case '\n': s = "end of line"; break;
    case TOK_NONE: s = "no token"; break;
    case TOK_ERROR: s = "invalid token"; break;
    case TOK_EOF: s = "end of file"; break;
    case TOK_ID: s = "name"; break;
    case TOK_NUM: s = "number"; break;
    case TOK_STR: s = "string"; break;
    case TOK_NE: s = "<>"; break;
    case TOK_LE: s = "<="; break;
    case TOK_GE: s = ">="; break;
    default: s = keyword_name(t); break;
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

static void test_token_name(CuTest* tc) {
  CuAssertTrue(tc, token_name(0) == NULL);
  CuAssertTrue(tc, token_name('A') == NULL);
  CuAssertStrEquals(tc, "<>", token_name(TOK_NE));
  CuAssertStrEquals(tc, "RESTORE", token_name(TOK_RESTORE));
}

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

CuSuite* token_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_token_name);
  SUITE_ADD_TEST(suite, test_identifier_token);
  SUITE_ADD_TEST(suite, test_keyword_prefix);
  return suite;
}

#endif // UNIT_TEST
