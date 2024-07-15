// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks
// Utility functions for BASIC arrays.

#include <stdbool.h>
#include <assert.h>
#include "arrays.h"
#include "utils.h"

static bool compute_total_elements(unsigned base, unsigned dimensions, unsigned max[], unsigned *elements) {
  assert(dimensions == 0 || max != NULL);
  assert(elements != NULL);
  *elements = 1;
  for (unsigned i = 0; i < dimensions; i++) {
    if (max[i] < base)
      return false;
    unsigned size = max[i] - base + 1;
    if (size > MAX_ELEMENTS / *elements)
      return false;
    *elements *= size;
  }
  return true;
}

static bool init_array_size(struct array_size * p, unsigned base, unsigned dimensions, unsigned max[]) {
  assert(p != NULL);

  if (dimensions < 1 || dimensions > MAX_DIMENSIONS)
    return false;

  unsigned elements;
  if (!compute_total_elements(base, dimensions, max, &elements))
    return false;

  p->base = base;
  p->dimensions = dimensions;
  for (unsigned i = 0; i < dimensions; i++)
    p->max[i] = max[i];
  p->elements = elements;
  return true;
}

static bool compute_element_offset(const struct array_size * p, unsigned dimensions, const unsigned indexes[], unsigned *offset) {
  assert(p != NULL);
  assert(offset != NULL);
  if (dimensions != p->dimensions)
    return false;
  for (unsigned i = 0; i < dimensions; i++) {
    if (indexes[i] < p->base || indexes[i] > p->max[i])
      return false;
  }
  switch (dimensions) {
    case 1:
      *offset = indexes[0] - p->base;
      break;
    case 2: {
      unsigned size1 = p->max[1] - p->base + 1;
      unsigned base1 = (indexes[0] - p->base) * size1;
      *offset = base1 + indexes[1] - p->base;
      break;
    }
    default:
      fatal("internal error: compute_numeric_offset: unsupported number of dimensions\n");
  }
  return true;
}

struct numeric_array * new_numeric_array(unsigned base, unsigned dimensions, unsigned max[]) {
  struct array_size size;
  if (!init_array_size(&size, base, dimensions, max))
    return NULL;

  struct numeric_array * p = ecalloc(1, sizeof *p + size.elements * sizeof p->val[0]);
  p->size = size;
  return p;
}

void delete_numeric_array(struct numeric_array * p) {
  efree(p);
}

bool compute_numeric_element(struct numeric_array * p, unsigned dimensions, const unsigned indexes[], double* *addr) {
  assert(p != NULL);
  assert(addr != NULL);
  unsigned offset;
  if (compute_element_offset(&p->size, dimensions, indexes, &offset)) {
    *addr = p->val + offset;
    return true;
  }
  return false;
}

struct string_array * new_string_array(unsigned base, unsigned dimensions, unsigned max[]) {
  struct array_size size;
  if (!init_array_size(&size, base, dimensions, max))
    return NULL;

  struct string_array * p = ecalloc(1, sizeof *p + size.elements * sizeof p->val[0]);
  p->size = size;
  return p;
}

void delete_string_array(struct string_array * p) {
  if (p) {
    for (unsigned i = 0; i < p->size.elements; i++)
      efree(p->val[i]);
    efree(p);
  }
}

bool compute_string_element(struct string_array * p, unsigned dimensions, const unsigned indexes[], char* * *addr) {
  unsigned offset;
  if (compute_element_offset(&p->size, dimensions, indexes, &offset)) {
    *addr = p->val + offset;
    return true;
  }
  return false;
}

#ifdef UNIT_TEST

#include "CuTest.h"

static void test_compute_total_elements(CuTest* tc) {
  unsigned elements;
  unsigned max[2];

  CuAssertIntEquals(tc, true, compute_total_elements(0, 0, NULL, &elements));
  CuAssertIntEquals(tc, 1, elements);

  max[0] = 7;
  CuAssertIntEquals(tc, true, compute_total_elements(1, 1, max, &elements));
  CuAssertIntEquals(tc, 7, elements);

  max[0] = 7;
  CuAssertIntEquals(tc, true, compute_total_elements(0, 1, max, &elements));
  CuAssertIntEquals(tc, 8, elements);

  max[0] = 0;
  CuAssertIntEquals(tc, true, compute_total_elements(0, 1, max, &elements));
  CuAssertIntEquals(tc, 1, elements);

  max[0] = 0;
  CuAssertIntEquals(tc, false, compute_total_elements(1, 1, max, &elements));

  max[0] = 3;
  max[1] = 5;
  CuAssertIntEquals(tc, true, compute_total_elements(1, 2, max, &elements));
  CuAssertIntEquals(tc, 15, elements);

  max[0] = 3;
  max[1] = 5;
  CuAssertIntEquals(tc, true, compute_total_elements(3, 2, max, &elements));
  CuAssertIntEquals(tc, 3, elements);

  max[0] = 2;
  max[1] = 5;
  CuAssertIntEquals(tc, false, compute_total_elements(3, 2, max, &elements));
}

static void test_init_array_size(CuTest* tc) {
  struct array_size size;
  unsigned max[2] = { 0, 0 };

  CuAssertIntEquals(tc, false, init_array_size(&size, 0, 0, max));
  CuAssertIntEquals(tc, false, init_array_size(&size, 0, 3, max));

  max[0] = 2;
  CuAssertIntEquals(tc, false, init_array_size(&size, 3, 1, max));

  // array(2..2)
  CuAssertIntEquals(tc, true, init_array_size(&size, 2, 1, max));
  CuAssertIntEquals(tc, 2, size.base);
  CuAssertIntEquals(tc, 1, size.dimensions);
  CuAssertIntEquals(tc, 2, size.max[0]);
  CuAssertIntEquals(tc, 1, size.elements);

  // array(1..3,1..5)
  max[0] = 3;
  max[1] = 5;
  CuAssertIntEquals(tc, true, init_array_size(&size, 1, 2, max));
  CuAssertIntEquals(tc, 1, size.base);
  CuAssertIntEquals(tc, 2, size.dimensions);
  CuAssertIntEquals(tc, 3, size.max[0]);
  CuAssertIntEquals(tc, 5, size.max[1]);
  CuAssertIntEquals(tc, 15, size.elements);
}

static void test_compute_element_offset(CuTest* tc) {
  struct array_size size;
  unsigned indexes[2];
  unsigned offset;

  // array(1..6)
  size.base = 1;
  size.dimensions = 1;
  size.max[0] = 6;
  size.elements = 6;

  // wrong dimensions
  indexes[0] = 1;
  CuAssertIntEquals(tc, false, compute_element_offset(&size, 2, indexes, &offset));

  // index too low
  indexes[0] = 0;
  CuAssertIntEquals(tc, false, compute_element_offset(&size, 1, indexes, &offset));

  // index too high
  indexes[0] = 7;
  CuAssertIntEquals(tc, false, compute_element_offset(&size, 1, indexes, &offset));

  // index minimum
  indexes[0] = 1;
  CuAssertIntEquals(tc, true, compute_element_offset(&size, 1, indexes, &offset));
  CuAssertIntEquals(tc, 0, offset);

  // index maximum
  indexes[0] = 6;
  CuAssertIntEquals(tc, true, compute_element_offset(&size, 1, indexes, &offset));
  CuAssertIntEquals(tc, 5, offset);

  // array(0..2,0..3)
  size.base = 0;
  size.dimensions = 2;
  size.max[0] = 2;
  size.max[1] = 3;
  size.elements = 24;

  // wrong dimensions
  indexes[0] = 0;
  indexes[1] = 0;
  CuAssertIntEquals(tc, false, compute_element_offset(&size, 1, indexes, &offset));

  // indexes minimum
  indexes[0] = 0;
  indexes[1] = 0;
  CuAssertIntEquals(tc, true, compute_element_offset(&size, 2, indexes, &offset));
  CuAssertIntEquals(tc, 0, offset);

  // indexes maximum
  indexes[0] = 2;
  indexes[1] = 3;
  CuAssertIntEquals(tc, true, compute_element_offset(&size, 2, indexes, &offset));
  CuAssertIntEquals(tc, 11, offset);

  // check first-index-major order (whether first index is row or column is up to client)
  indexes[0] = 0;
  indexes[1] = 1;
  CuAssertIntEquals(tc, true, compute_element_offset(&size, 2, indexes, &offset));
  CuAssertIntEquals(tc, 1, offset);

  indexes[0] = 0;
  indexes[1] = 3;
  CuAssertIntEquals(tc, true, compute_element_offset(&size, 2, indexes, &offset));
  CuAssertIntEquals(tc, 3, offset);

  indexes[0] = 1;
  indexes[1] = 0;
  CuAssertIntEquals(tc, true, compute_element_offset(&size, 2, indexes, &offset));
  CuAssertIntEquals(tc, 4, offset);
}

static void test_numeric_array(CuTest* tc) {
  struct numeric_array * p;
  unsigned max[2];
  unsigned indexes[2];
  double *addr;

  max[0] = 1;
  max[1] = 1;
  // Too few dimensions
  CuAssertPtrEquals(tc, NULL, new_numeric_array(0, 0, max));
  // Too many dimensions
  CuAssertPtrEquals(tc, NULL, new_numeric_array(0, 3, max));
  // max > base
  CuAssertPtrEquals(tc, NULL, new_numeric_array(2, 2, max));

  // array(0..4)
  max[0] = 4;
  p = new_numeric_array(0, 1, max);

  CuAssertPtrNotNull(tc, p);
  CuAssertIntEquals(tc, 0, p->size.base);
  CuAssertIntEquals(tc, 1, p->size.dimensions);
  CuAssertIntEquals(tc, 4, p->size.max[0]);
  CuAssertIntEquals(tc, 5, p->size.elements);
  CuAssertDblEquals(tc, 0, p->val[0], 0);
  CuAssertDblEquals(tc, 0, p->val[4], 0);

  indexes[0] = 0;
  CuAssertIntEquals(tc, true, compute_numeric_element(p, 1, indexes, &addr));
  CuAssertPtrEquals(tc, p->val, addr);

  indexes[0] = 4;
  CuAssertIntEquals(tc, true, compute_numeric_element(p, 1, indexes, &addr));
  CuAssertPtrEquals(tc, p->val + 4, addr);

  indexes[0] = 5;
  CuAssertIntEquals(tc, false, compute_numeric_element(p, 1, indexes, &addr));

  delete_numeric_array(p);

  // array(1..2,1..3)
  max[0] = 2;
  max[1] = 3;
  p = new_numeric_array(1, 2, max);

  CuAssertPtrNotNull(tc, p);
  CuAssertIntEquals(tc, 1, p->size.base);
  CuAssertIntEquals(tc, 2, p->size.dimensions);
  CuAssertIntEquals(tc, 2, p->size.max[0]);
  CuAssertIntEquals(tc, 3, p->size.max[1]);
  CuAssertIntEquals(tc, 6, p->size.elements);
  CuAssertDblEquals(tc, 0, p->val[0], 0);
  CuAssertDblEquals(tc, 0, p->val[5], 0);

  indexes[0] = 0;
  indexes[1] = 0;
  CuAssertIntEquals(tc, false, compute_numeric_element(p, 2, indexes, &addr));

  indexes[0] = 1;
  indexes[1] = 1;
  CuAssertIntEquals(tc, true, compute_numeric_element(p, 2, indexes, &addr));
  CuAssertPtrEquals(tc, p->val, addr);

  indexes[0] = 2;
  indexes[1] = 3;
  CuAssertIntEquals(tc, true, compute_numeric_element(p, 2, indexes, &addr));
  CuAssertPtrEquals(tc, p->val + 5, addr);

  indexes[0] = 3;
  indexes[1] = 3;
  CuAssertIntEquals(tc, false, compute_numeric_element(p, 2, indexes, &addr));

  indexes[0] = 1;
  indexes[1] = 1;
  CuAssertIntEquals(tc, false, compute_numeric_element(p, 1, indexes, &addr));

  delete_numeric_array(p);
}

static void test_string_array(CuTest* tc) {
  struct string_array * p;
  unsigned max[2];
  unsigned indexes[2];
  char* *addr;

  max[0] = 1;
  max[1] = 1;
  // Too few dimensions
  CuAssertPtrEquals(tc, NULL, new_string_array(0, 0, max));
  // Too many dimensions
  CuAssertPtrEquals(tc, NULL, new_string_array(0, 3, max));
  // max > base
  CuAssertPtrEquals(tc, NULL, new_string_array(2, 2, max));

  // array(0..4)
  max[0] = 4;
  p = new_string_array(0, 1, max);

  CuAssertPtrNotNull(tc, p);
  CuAssertIntEquals(tc, 0, p->size.base);
  CuAssertIntEquals(tc, 1, p->size.dimensions);
  CuAssertIntEquals(tc, 4, p->size.max[0]);
  CuAssertIntEquals(tc, 5, p->size.elements);
  CuAssertPtrEquals(tc, NULL, p->val[0]);
  CuAssertPtrEquals(tc, NULL, p->val[4]);

  indexes[0] = 0;
  CuAssertIntEquals(tc, true, compute_string_element(p, 1, indexes, &addr));
  CuAssertPtrEquals(tc, p->val, addr);

  indexes[0] = 4;
  CuAssertIntEquals(tc, true, compute_string_element(p, 1, indexes, &addr));
  CuAssertPtrEquals(tc, p->val + 4, addr);

  indexes[0] = 5;
  CuAssertIntEquals(tc, false, compute_string_element(p, 1, indexes, &addr));

  delete_string_array(p);

  // array(1..2,1..3)
  max[0] = 2;
  max[1] = 3;
  p = new_string_array(1, 2, max);

  CuAssertPtrNotNull(tc, p);
  CuAssertIntEquals(tc, 1, p->size.base);
  CuAssertIntEquals(tc, 2, p->size.dimensions);
  CuAssertIntEquals(tc, 2, p->size.max[0]);
  CuAssertIntEquals(tc, 3, p->size.max[1]);
  CuAssertIntEquals(tc, 6, p->size.elements);
  CuAssertPtrEquals(tc, NULL, p->val[0]);
  CuAssertPtrEquals(tc, NULL, p->val[5]);

  indexes[0] = 0;
  indexes[1] = 0;
  CuAssertIntEquals(tc, false, compute_string_element(p, 2, indexes, &addr));

  indexes[0] = 1;
  indexes[1] = 1;
  CuAssertIntEquals(tc, true, compute_string_element(p, 2, indexes, &addr));
  CuAssertPtrEquals(tc, p->val, addr);

  indexes[0] = 2;
  indexes[1] = 3;
  CuAssertIntEquals(tc, true, compute_string_element(p, 2, indexes, &addr));
  CuAssertPtrEquals(tc, p->val + 5, addr);

  indexes[0] = 3;
  indexes[1] = 3;
  CuAssertIntEquals(tc, false, compute_string_element(p, 2, indexes, &addr));

  indexes[0] = 1;
  indexes[1] = 1;
  CuAssertIntEquals(tc, false, compute_string_element(p, 1, indexes, &addr));

  delete_string_array(p);
}

CuSuite* arrays_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_compute_total_elements);
  SUITE_ADD_TEST(suite, test_init_array_size);
  SUITE_ADD_TEST(suite, test_compute_element_offset);
  SUITE_ADD_TEST(suite, test_numeric_array);
  SUITE_ADD_TEST(suite, test_string_array);
  return suite;
}

#endif
