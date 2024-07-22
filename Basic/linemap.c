// Legacy BASIC
// Copyright (c) 2024 Nigel Perks
// Map Basic line number to value.

#include <assert.h>
#include "linemap.h"
#include "utils.h"

// Mapping of one Basic line number to a value
struct line_node {
  unsigned basic_line;
  unsigned val;
  struct line_node * next;
};

#define LINE_HASH_SIZE (397)

struct line_map {
  struct line_node * hash[LINE_HASH_SIZE];
  struct line_node * nodes;
  unsigned allocated;
  unsigned count;
};

LINE_MAP* new_line_map(unsigned lines) {
  LINE_MAP* map = ecalloc(1, sizeof *map); // initialise hash table node pointers to null
  if (lines > 0)
    map->nodes = ecalloc(lines, sizeof map->nodes[0]);
  map->allocated = lines;
  map->count = 0;
  return map;
}

void delete_line_map(LINE_MAP* map) {
  if (map) {
    efree(map->nodes);
    efree(map);
  }
}

bool insert_line_mapping(LINE_MAP* map, unsigned basic_line, unsigned val) {
  assert(map->count <= map->allocated);
  if (map->count == map->allocated)
    return false;

  struct line_node * node = map->nodes + map->count;
  node->basic_line = basic_line;
  node->val = val;
  unsigned h = node->basic_line % LINE_HASH_SIZE;
  node->next = map->hash[h];
  map->hash[h] = node;
  map->count++;
  return true;
}

bool lookup_line_mapping(const LINE_MAP* map, unsigned basic_line, unsigned *val) {
  assert(map != NULL);
  assert(val != NULL);
  unsigned h = basic_line % LINE_HASH_SIZE;
  for (const struct line_node * node = map->hash[h]; node; node = node->next) {
    if (node->basic_line == basic_line) {
      *val = node->val;
      return true;
    }
  }
  return false;
}


#ifdef UNIT_TEST

#include "CuTest.h"

static void test_new_map(CuTest* tc) {
  LINE_MAP* map;

  // No lines
  map = new_line_map(0);
  CuAssertPtrNotNull(tc, map);
  CuAssertIntEquals(tc, 0, map->allocated);
  CuAssertIntEquals(tc, 0, map->count);
  CuAssertPtrEquals(tc, NULL, map->nodes);
  for (unsigned i = 0; i < LINE_HASH_SIZE; i++)
    CuAssertPtrEquals(tc, NULL, map->hash[i]);
  delete_line_map(map);

  // Lines
  map = new_line_map(4);
  CuAssertPtrNotNull(tc, map);
  CuAssertIntEquals(tc, 4, map->allocated);
  CuAssertIntEquals(tc, 0, map->count);
  CuAssertPtrNotNull(tc, map->nodes);
  for (unsigned i = 0; i < 4; i++) {
    CuAssertIntEquals(tc, 0, map->nodes[i].basic_line);
    CuAssertIntEquals(tc, 0, map->nodes[i].val);
    CuAssertPtrEquals(tc, NULL, map->nodes[i].next);
  }
  for (unsigned i = 0; i < LINE_HASH_SIZE; i++)
    CuAssertPtrEquals(tc, NULL, map->hash[i]);
  delete_line_map(map);
}

static void test_lookup(CuTest* tc) {
  LINE_MAP* map;
  bool succ;
  unsigned val;

  map = new_line_map(2);

  succ = lookup_line_mapping(map, 10, &val);
  CuAssertIntEquals(tc, false, succ);

  succ = insert_line_mapping(map, 10, 23);
  CuAssertIntEquals(tc, true, succ);
  val = 0;
  succ = lookup_line_mapping(map, 10, &val);
  CuAssertIntEquals(tc, true, succ);
  CuAssertIntEquals(tc, 23, val);

  succ = insert_line_mapping(map, 1000, 77);
  CuAssertIntEquals(tc, true, succ);
  val = 0;
  succ = lookup_line_mapping(map, 10, &val);
  CuAssertIntEquals(tc, true, succ);
  CuAssertIntEquals(tc, 23, val);
  succ = lookup_line_mapping(map, 1000, &val);
  CuAssertIntEquals(tc, true, succ);
  CuAssertIntEquals(tc, 77, val);

  succ = lookup_line_mapping(map, 230, &val);
  CuAssertIntEquals(tc, false, succ);

  delete_line_map(map);
}

CuSuite* linemap_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_new_map);
  SUITE_ADD_TEST(suite, test_lookup);
  return suite;
}

#endif
