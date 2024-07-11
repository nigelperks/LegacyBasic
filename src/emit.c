// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#include <assert.h>
#include "emit.h"
#include "utils.h"

unsigned emit(BCODE* bcode, unsigned op) {
  BINST* i = bcode_next(bcode, op);
  return (unsigned)(i - bcode->inst);
}

void emit_line(BCODE* bcode, unsigned op, unsigned line) {
  BINST* i = bcode_next(bcode, op);
  i->u.line = line;
}

void emit_num(BCODE* bcode, unsigned op, double num) {
  BINST* i = bcode_next(bcode, op);
  i->u.num = num;
}

void emit_str(BCODE* bcode, unsigned op, const char* str) {
  BINST* i = bcode_next(bcode, op);
  i->u.str = estrdup(str);
}

void emit_str_ptr(BCODE* bcode, unsigned op, char* str) {
  BINST* i = bcode_next(bcode, op);
  i->u.str = str;
}

void emit_var(BCODE* bcode, unsigned op, unsigned name) {
  BINST* i = bcode_next(bcode, op);
  i->u.name = name;
}

unsigned emit_param(BCODE* bcode, unsigned op, unsigned name, unsigned params) {
  BINST* i = bcode_next(bcode, op);
  i->u.param.name = name;
  i->u.param.params = params;
  return (unsigned)(i - bcode->inst);
}

unsigned emit_count(BCODE* bcode, unsigned op, unsigned count) {
  BINST* i = bcode_next(bcode, op);
  i->u.count = count;
  return (unsigned)(i - bcode->inst);
}

static void check_index(const BCODE* bcode, unsigned index) {
  if (index >= bcode->used)
    fatal("B-code index out of range\n");
}

void patch_opcode(BCODE* bcode, unsigned index, unsigned op) {
  check_index(bcode, index);
  bcode->inst[index].op = op;
}

void patch_count(BCODE* bcode, unsigned index, unsigned count) {
  check_index(bcode, index);
  bcode->inst[index].u.count = count;
}


#ifdef UNIT_TEST

#include "CuTest.h"

static void test_emit(CuTest* tc) {
  static const char CODE[] = "1 REM\n";
  SOURCE* source = load_source_string(CODE, "test");
  BCODE* bcode = new_bcode();
  CuAssertIntEquals(tc, 0, bcode->used);

  emit(bcode, B_ADD);
  CuAssertIntEquals(tc, 1, bcode->used);
  CuAssertTrue(tc, bcode->allocated >= 1);
  CuAssertIntEquals(tc, B_ADD, bcode->inst[0].op);

  emit_line(bcode, B_GOTO, 1000);
  CuAssertIntEquals(tc, 2, bcode->used);
  CuAssertIntEquals(tc, B_GOTO, bcode->inst[1].op);
  CuAssertIntEquals(tc, 1000, bcode->inst[1].u.line);

  emit_num(bcode, B_PUSH_NUM, 1.23e-13);
  CuAssertIntEquals(tc, 3, bcode->used);
  CuAssertIntEquals(tc, B_PUSH_NUM, bcode->inst[2].op);
  CuAssertDblEquals(tc, 1.23e-13, bcode->inst[2].u.num, 0);

  static const char STR[] = "pilchards";
  emit_str(bcode, B_PUSH_STR, STR);
  CuAssertIntEquals(tc, 4, bcode->used);
  CuAssertIntEquals(tc, B_PUSH_STR, bcode->inst[3].op);
  CuAssertStrEquals(tc, STR, bcode->inst[3].u.str);
  CuAssertTrue(tc, bcode->inst[3].u.str != STR);

  char* ptr = estrdup("sardines");
  emit_str_ptr(bcode, B_DATA, ptr);
  CuAssertIntEquals(tc, 5, bcode->used);
  CuAssertIntEquals(tc, B_DATA, bcode->inst[4].op);
  CuAssertPtrEquals(tc, ptr, bcode->inst[4].u.str);

  emit_var(bcode, B_PARAM, 13);
  CuAssertIntEquals(tc, 6, bcode->used);
  CuAssertIntEquals(tc, B_PARAM, bcode->inst[5].op);
  CuAssertIntEquals(tc, 13, bcode->inst[5].u.name);

  emit_param(bcode, B_DIM_NUM, 17, 3);
  CuAssertIntEquals(tc, 7, bcode->used);
  CuAssertIntEquals(tc, B_DIM_NUM, bcode->inst[6].op);
  CuAssertIntEquals(tc, 17, bcode->inst[6].u.param.name);
  CuAssertIntEquals(tc, 3, bcode->inst[6].u.param.params);

  unsigned i = emit_count(bcode, B_ON_GOTO, 5);
  CuAssertIntEquals(tc, 8, bcode->used);
  CuAssertTrue(tc, bcode->allocated >= 8);
  CuAssertIntEquals(tc, 7, i);
  CuAssertIntEquals(tc, B_ON_GOTO, bcode->inst[i].op);
  CuAssertIntEquals(tc, 5, bcode->inst[i].u.count);

  patch_count(bcode, i, 21);
  CuAssertIntEquals(tc, 8, bcode->used);
  CuAssertIntEquals(tc, B_ON_GOTO, bcode->inst[i].op);
  CuAssertIntEquals(tc, 21, bcode->inst[i].u.count);

  delete_bcode(bcode);
  delete_source(source);
}

CuSuite* emit_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_emit);
  return suite;
}

#endif
