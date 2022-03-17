// Legacy BASIC
// String list.
// Copyright (c) 2021-2 Nigel Perks

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "stringlist.h"
#include "utils.h"
#include "os.h"

void init_stringlist(STRINGLIST* p) {
  p->strings = NULL;
  p->allocated = 0;
  p->used = 0;
}

void deinit_stringlist(STRINGLIST* p) {
  if (p) {
    for (unsigned i = 0; i < p->used; i++)
      efree(p->strings[i]);
    efree(p->strings);
    p->strings = NULL;
    p->allocated = 0;
    p->used = 0;
  }
}

STRINGLIST* new_stringlist(void) {
  STRINGLIST* p = emalloc(sizeof *p);
  init_stringlist(p);
  return p;
}

void delete_stringlist(STRINGLIST* p) {
  if (p) {
    deinit_stringlist(p);
    efree(p);
  }
}

unsigned stringlist_count(const STRINGLIST* list) {
  assert(list != NULL);
  return list->used;
}

const char* stringlist_item(const STRINGLIST* list, unsigned i) {
  assert(list != NULL);
  assert(i < list->used);
  assert(list->strings != NULL);
  return list->strings[i];
}

unsigned stringlist_append(STRINGLIST* list, const char* s) {
  assert(list != NULL);
  assert(list->used <= list->allocated);
  if (list->used == list->allocated) {
    unsigned new_allocated = (list->allocated) ? 2 * list->allocated : 8;
    char* * t = erealloc(list->strings, new_allocated * sizeof (list->strings[0]));
    assert(t != NULL);
    list->strings = t;
    list->allocated = new_allocated;
  }
  assert(list->used < list->allocated);
  unsigned i = list->used++;
  list->strings[i] = estrdup(s);
  return i;
}

unsigned stringlist_entry_case_insensitive(STRINGLIST* list, const char* s) {
  assert(list != NULL);
  for (unsigned i = 0; i < list->used; i++) {
    if (STRICMP(list->strings[i], s) == 0)
      return i;
  }
  return stringlist_append(list, s);
}


#ifdef UNIT_TEST

#include "CuTest.h"

static void test_init(CuTest* tc) {
  STRINGLIST list;

  memset(&list, 0xff, sizeof list);
  init_stringlist(&list);
  CuAssertPtrEquals(tc, NULL, list.strings);
  CuAssertIntEquals(tc, 0, list.allocated);
  CuAssertIntEquals(tc, 0, list.used);
}

static void test_deinit(CuTest* tc) {
  STRINGLIST list;

  deinit_stringlist(NULL);

  list.strings = emalloc(4 * sizeof list.strings[0]);
  list.allocated = 4;
  list.strings[0] = estrdup("hello");
  list.strings[1] = estrdup("goodbye");
  list.used = 2;
  deinit_stringlist(&list);
  CuAssertPtrEquals(tc, NULL, list.strings);
  CuAssertIntEquals(tc, 0, list.allocated);
  CuAssertIntEquals(tc, 0, list.used);
}

static void test_new(CuTest* tc) {
  STRINGLIST* p;

  p = new_stringlist();
  CuAssertPtrNotNull(tc, p);
  CuAssertPtrEquals(tc, NULL, p->strings);
  CuAssertIntEquals(tc, 0, p->allocated);
  CuAssertIntEquals(tc, 0, p->used);

  delete_stringlist(p);

  delete_stringlist(NULL);
}

static void test_append(CuTest* tc) {
  STRINGLIST list;

  init_stringlist(&list);

  CuAssertIntEquals(tc, 0, stringlist_count(&list));

  CuAssertIntEquals(tc, 0, stringlist_append(&list, "ShoeMaker"));
  CuAssertPtrNotNull(tc, list.strings);
  CuAssertTrue(tc, list.allocated >= 1);
  CuAssertIntEquals(tc, 1, list.used);
  CuAssertIntEquals(tc, 1, stringlist_count(&list));
  CuAssertStrEquals(tc, "ShoeMaker", stringlist_item(&list, 0));

  CuAssertIntEquals(tc, 0, stringlist_entry_case_insensitive(&list, "SHOEMAKER"));
  CuAssertIntEquals(tc, 1, list.used);
  CuAssertIntEquals(tc, 1, stringlist_count(&list));

  CuAssertIntEquals(tc, 1, stringlist_entry_case_insensitive(&list, "Elephant"));
  CuAssertIntEquals(tc, 2, list.used);
  CuAssertIntEquals(tc, 2, stringlist_count(&list));
  CuAssertStrEquals(tc, "Elephant", stringlist_item(&list, 1));

  deinit_stringlist(&list);
}

CuSuite* stringlist_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_init);
  SUITE_ADD_TEST(suite, test_deinit);
  SUITE_ADD_TEST(suite, test_new);
  SUITE_ADD_TEST(suite, test_append);
  return suite;
}

#endif
