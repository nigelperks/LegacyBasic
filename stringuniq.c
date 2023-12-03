// Maintain a set of unique strings by means of a binary tree.
// Copyright (c) 2023 Nigel Perks

#include <assert.h>
#include "stringuniq.h"
#include "utils.h"
#include "os.h"

struct node {
  char* s;
  struct node * left;
  struct node * right;
};

static struct node * new_node(const char* s) {
  struct node * n = emalloc(sizeof *n);
  n->s = estrdup(s);
  n->left = NULL;
  n->right = NULL;
  return n;
}

static void delete_tree(struct node * n) {
  if (n) {
    delete_tree(n->left);
    delete_tree(n->right);
    efree(n->s);
    efree(n);
  }
}

static struct node * insert_node(struct node * n, const char* s) {
  if (n == NULL)
    return new_node(s);
  assert(n->s != NULL);
  int cmp = STRICMP(s, n->s);
  if (cmp < 0)
    n->left = insert_node(n->left, s);
  else if (cmp > 0)
    n->right = insert_node(n->right, s);
  return n;
}

static void traverse_tree(const struct node * n, void (*callback)(const char*)) {
  if (n) {
    traverse_tree(n->left, callback);
    callback(n->s);
    traverse_tree(n->right, callback);
  }
}

struct unique_strings {
  struct node * root;
};

UNIQUE_STRINGS* new_unique_strings(void) {
  UNIQUE_STRINGS* p = emalloc(sizeof *p);
  p->root = NULL;
  return p;
}

void delete_unique_strings(UNIQUE_STRINGS* p) {
  if (p) {
    delete_tree(p->root);
    efree(p);
  }
}

void insert_unique_string(UNIQUE_STRINGS* p, const char* s) {
  assert(p != NULL);
  assert(s != NULL);
  p->root = insert_node(p->root, s);
}

void traverse_unique_strings(const UNIQUE_STRINGS* p, void (*callback)(const char*)) {
  traverse_tree(p->root, callback);
}

#ifdef UNIT_TEST

#include "CuTest.h"
#include "stringlist.h"

static void test_new_node(CuTest* tc) {
  const char* name = "hello";
  struct node * n = new_node(name);
  CuAssertPtrNotNull(tc, n);
  CuAssertTrue(tc, n->s != NULL && n->s != name);
  CuAssertStrEquals(tc, name, n->s);
  CuAssertPtrEquals(tc, NULL, n->left);
  CuAssertPtrEquals(tc, NULL, n->right);
  delete_tree(n);
}

static void test_insert_node(CuTest* tc) {
  // null input: new root
  struct node * n = insert_node(NULL, "hello");
  CuAssertPtrNotNull(tc, n);
  CuAssertStrEquals(tc, "hello", n->s);
  CuAssertPtrEquals(tc, NULL, n->left);
  CuAssertPtrEquals(tc, NULL, n->right);

  // new left
  insert_node(n, "goodbye");
  CuAssertStrEquals(tc, "hello", n->s);
  CuAssertPtrNotNull(tc, n->left);
  CuAssertPtrEquals(tc, NULL, n->right);
  CuAssertStrEquals(tc, "goodbye", n->left->s);
  CuAssertPtrEquals(tc, NULL, n->left->left);
  CuAssertPtrEquals(tc, NULL, n->left->right);

  // new right
  insert_node(n, "lemon");
  CuAssertStrEquals(tc, "hello", n->s);
  CuAssertPtrNotNull(tc, n->left);
  CuAssertPtrNotNull(tc, n->right);
  CuAssertStrEquals(tc, "lemon", n->right->s);
  CuAssertPtrEquals(tc, NULL, n->right->left);
  CuAssertPtrEquals(tc, NULL, n->right->right);

  // third level
  insert_node(n, "jolly");
  struct node * j = n->right->left;
  CuAssertPtrNotNull(tc, j);
  CuAssertStrEquals(tc, "jolly", j->s);
  CuAssertPtrEquals(tc, NULL, j->left);
  CuAssertPtrEquals(tc, NULL, j->right);
  
  // clean up
  delete_tree(n);
}

static STRINGLIST* list = NULL;

static void append(const char* s) {
  stringlist_append(list, s);
}

static void test_traverse_tree(CuTest* tc) {
  struct node * root = insert_node(NULL, "melon");
  insert_node(root, "banana");
  insert_node(root, "cabbage");
  insert_node(root, "apple");
  insert_node(root, "nufruit");

  list = new_stringlist();
  traverse_tree(root, append);
  CuAssertIntEquals(tc, 5, list->used);
  CuAssertStrEquals(tc, "apple", list->strings[0]);
  CuAssertStrEquals(tc, "banana", list->strings[1]);
  CuAssertStrEquals(tc, "cabbage", list->strings[2]);
  CuAssertStrEquals(tc, "melon", list->strings[3]);
  CuAssertStrEquals(tc, "nufruit", list->strings[4]);
  delete_stringlist(list);
  list = NULL;

  delete_tree(root);
}

CuSuite* stringuniq_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_new_node);
  SUITE_ADD_TEST(suite, test_insert_node);
  SUITE_ADD_TEST(suite, test_traverse_tree);
  return suite;
}

#endif
