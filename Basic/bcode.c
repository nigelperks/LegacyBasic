// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "bcode.h"
#include "emit.h"
#include "utils.h"

static const struct {
  char* name;
  int format;
} ops[] = {
  { "NOP", BF_IMPLICIT },
  // source
  { "LINE", BF_SOURCE_LINE },
  // whole environment
  { "CLEAR", BF_IMPLICIT },
  // number
  { "PUSH-NUM", BF_NUM },
  { "POP-NUM", BF_IMPLICIT },
  { "GET-SIMPLE-NUM", BF_VAR },
  { "SET-SIMPLE-NUM", BF_VAR },
  { "DIM-NUM", BF_PARAM },
  { "GET-PARAM-NUM", BF_PARAM },
  { "SET-ARRAY-NUM", BF_PARAM },
  { "ADD", BF_IMPLICIT },
  { "SUB", BF_IMPLICIT },
  { "MUL", BF_IMPLICIT },
  { "DIV", BF_IMPLICIT },
  { "POW", BF_IMPLICIT },
  { "EQ-NUM", BF_IMPLICIT },
  { "LT-NUM", BF_IMPLICIT },
  { "GT-NUM", BF_IMPLICIT },
  { "NE-NUM", BF_IMPLICIT },
  { "LE-NUM", BF_IMPLICIT },
  { "GE-NUM", BF_IMPLICIT },
  { "OR", BF_IMPLICIT },
  { "AND", BF_IMPLICIT },
  { "NOT", BF_IMPLICIT },
  { "NEG", BF_IMPLICIT },
  // string
  { "PUSH-STR", BF_STR },
  { "POP-STR", BF_IMPLICIT },
  { "SET-SIMPLE-STR", BF_VAR },
  { "GET-SIMPLE-STR", BF_VAR },
  { "DIM-STR", BF_PARAM },
  { "GET-ARRAY-STR", BF_PARAM },
  { "SET-ARRAY-STR", BF_PARAM },
  { "EQ-STR", BF_IMPLICIT },
  { "NE-STR", BF_IMPLICIT },
  { "LT-STR", BF_IMPLICIT },
  { "GT-STR", BF_IMPLICIT },
  { "LE-STR", BF_IMPLICIT },
  { "GE-STR", BF_IMPLICIT },
  { "CONCAT", BF_IMPLICIT },
  // control flow
  { "END", BF_IMPLICIT },
  { "STOP", BF_IMPLICIT },
  { "GOTO", BF_BASIC_LINE },
  { "GOTRUE", BF_BASIC_LINE },
  { "GOSUB", BF_BASIC_LINE },
  { "RETURN", BF_IMPLICIT },
  { "FOR", BF_VAR },
  { "NEXT-VAR", BF_VAR },
  { "NEXT-IMP", BF_IMPLICIT },
  { "DEF", BF_PARAM },
  { "PARAM", BF_VAR },
  { "END-DEF", BF_IMPLICIT },
  { "ON-GOTO", BF_COUNT },
  { "ON-GOSUB", BF_COUNT },
  { "ON-LINE", BF_BASIC_LINE },
  { "IF-THEN", BF_IMPLICIT },
  { "IF-ELSE", BF_IMPLICIT },
  { "ELSE", BF_IMPLICIT },
  // output
  { "PRINT-LN", BF_IMPLICIT },
  { "PRINT-SPC", BF_IMPLICIT },
  { "PRINT-TAB", BF_IMPLICIT },
  { "PRINT-COMMA", BF_IMPLICIT },
  { "PRINT-NUM", BF_IMPLICIT },
  { "PRINT-STR", BF_IMPLICIT },
  { "CLS", BF_IMPLICIT },
  // input
  { "INPUT-BUF", BF_STR },
  { "INPUT-END", BF_IMPLICIT },
  { "INPUT-SEP", BF_IMPLICIT },
  { "INPUT-NUM", BF_PARAM },
  { "INPUT-STR", BF_PARAM },
  { "INPUT-LINE", BF_PARAM },
  // inline data
  { "DATA", BF_STR },
  { "READ-NUM", BF_PARAM },
  { "READ-STR", BF_PARAM },
  { "RESTORE", BF_IMPLICIT },
  { "RESTORE-LINE", BF_BASIC_LINE },
  // random numbers
  { "RAND", BF_IMPLICIT },
  { "SEED", BF_IMPLICIT },
  // builtins
  { "ABS", BF_IMPLICIT },
  { "ASC", BF_IMPLICIT },
  { "ATN", BF_IMPLICIT },
  { "CHR", BF_IMPLICIT },
  { "COS", BF_IMPLICIT },
  { "EXP", BF_IMPLICIT },
  { "INKEY", BF_IMPLICIT },
  { "INT", BF_IMPLICIT },
  { "LEFT", BF_IMPLICIT },
  { "LEN", BF_IMPLICIT },
  { "LOG", BF_IMPLICIT },
  { "MID3", BF_IMPLICIT },
  { "RIGHT", BF_IMPLICIT },
  { "RND", BF_IMPLICIT },
  { "SGN", BF_IMPLICIT },
  { "SIN", BF_IMPLICIT },
  { "SQR", BF_IMPLICIT },
  { "STR", BF_IMPLICIT },
  { "TAN", BF_IMPLICIT },
  { "TIME-STR", BF_IMPLICIT },
  { "VAL", BF_IMPLICIT },
};

static void check(unsigned opcode) {
  if (opcode >= sizeof ops / sizeof ops[0])
    fatal("internal error: opcode out of range\n");
}

const char* bcode_name(int opcode) {
  check(opcode);
  return ops[opcode].name;
}

int bcode_format(int opcode) {
  check(opcode);
  return ops[opcode].format;
}

BCODE* new_bcode(void) {
  BCODE* p = emalloc(sizeof *p);
  p->inst = NULL;
  p->allocated = 0;
  p->used = 0;
  p->has_data = false;
  return p;
}

void delete_bcode(BCODE* p) {
  if (p) {
    for (unsigned i = 0; i < p->used; i++) {
      if (p->inst[i].op < sizeof ops / sizeof ops[0] && ops[p->inst[i].op].format == BF_STR)
        efree(p->inst[i].u.str);
    }
    efree(p->inst);
    efree(p);
  }
}

const BINST* bcode_latest(const BCODE* p) {
  assert(p != NULL);
  return p->used ? &p->inst[p->used - 1] : NULL;
}

BINST* bcode_next(BCODE* p, unsigned op) {
  assert(p != NULL);
  assert(p->used <= p->allocated);
  if (p->used == p->allocated) {
    p->allocated = p->allocated ? 2 * p->allocated : 128;
    p->inst = erealloc(p->inst, p->allocated * sizeof p->inst[0]);
  }
  assert(p->used < p->allocated);
  BINST* i = &p->inst[p->used++];
  i->op = op;
  if (op == B_DATA)
    p->has_data = true;
  return i;
}

static unsigned def_end(const BCODE* code, unsigned pc) {
  assert(code != NULL);
  assert(pc < code->used);

  while (pc < code->used && code->inst[pc].op != B_END_DEF)
    pc++;
  return pc;
}

static BINST copy_inst(const BINST* i) {
  BINST j = *i;
  assert(j.op < sizeof ops / sizeof ops[0]);
  if (ops[j.op].format == BF_STR)
    j.u.str = estrdup(j.u.str);
  return j;
}

BCODE* bcode_copy_def(const BCODE* src, unsigned start) {
  unsigned end = def_end(src, start);
  unsigned len = end - start;
  unsigned size = len + 1;
  BCODE* dst = new_bcode();
  dst->inst = emalloc(size * sizeof dst->inst[0]);
  dst->allocated = size;
  for (unsigned i = 0; i < len; i++)
    dst->inst[i] = copy_inst(&src->inst[start + i]);
  dst->inst[len].op = B_END_DEF;
  dst->used = size;
  return dst;
}

// Compute the number of Basic lines, i.e. the number of source lines,
// referred to in the B-code.
static unsigned line_count(const BCODE* bc) {
  assert(bc != NULL);
  unsigned n = 0;
  for (unsigned i = 0; i < bc->used; i++) {
    if (bc->inst[i].op == B_SOURCE_LINE)
      n++;
  }
  return n;
}

// Build index of Basic line numbers for faster lookup
LINE_MAP* bcode_index(const BCODE* bc, const SOURCE* source) {
  assert(bc != NULL);
  assert(source != NULL);

  LINE_MAP* idx = new_line_map(line_count(bc));

  for (unsigned i = 0; i < bc->used; i++) {
    if (bc->inst[i].op == B_SOURCE_LINE) {
      unsigned basic_line = source_linenum(source, bc->inst[i].u.source_line);
      insert_line_mapping(idx, basic_line, i);
    }
  }

  return idx;
}

#ifdef UNIT_TEST

#include "CuTest.h"

static void test_bcode(CuTest* tc) {
  static const char CODE[] = "10 PRINT\n20 PRINT\n";
  SOURCE* const src = load_source_string(CODE, "test");
  BCODE* p;
  BINST* i;

  p = new_bcode();
  CuAssertPtrNotNull(tc, p);
  CuAssertPtrEquals(tc, NULL, p->inst);
  CuAssertIntEquals(tc, 0, p->allocated);
  CuAssertIntEquals(tc, 0, p->used);

  i = bcode_next(p, B_ADD);
  CuAssertPtrNotNull(tc, i);
  CuAssertPtrNotNull(tc, p->inst);
  CuAssertTrue(tc, p->allocated >= 1);
  CuAssertIntEquals(tc, 1, p->used);
  CuAssertIntEquals(tc, B_ADD, p->inst[0].op);

  i = bcode_next(p, B_SOURCE_LINE);
  CuAssertPtrNotNull(tc, i);
  CuAssertIntEquals(tc, 2, p->used);

  delete_bcode(p);
  delete_source(src);
}

static void test_def_end(CuTest* tc) {
  BCODE* bc = new_bcode();

  emit(bc, B_END_DEF);
  CuAssertIntEquals(tc, 0, def_end(bc, 0));

  emit(bc, B_ADD);
  CuAssertIntEquals(tc, 2, def_end(bc, 1));

  emit(bc, B_SUB);
  emit(bc, B_END_DEF);
  CuAssertIntEquals(tc, 3, def_end(bc, 1));

  delete_bcode(bc);
}

static void test_bcode_copy_def(CuTest* tc) {
  BCODE* src;
  BCODE* dst;

  src = new_bcode();
  emit(src, B_ADD);
  emit(src, B_SUB);
  emit(src, B_END_DEF);
  emit(src, B_MUL);

  // implicit end
  dst = bcode_copy_def(src, 3);
  CuAssertPtrNotNull(tc, dst);
  CuAssertIntEquals(tc, 2, dst->allocated);
  CuAssertIntEquals(tc, 2, dst->used);
  CuAssertPtrNotNull(tc, dst->inst);
  CuAssertIntEquals(tc, B_MUL, dst->inst[0].op);
  CuAssertIntEquals(tc, B_END_DEF, dst->inst[1].op);
  delete_bcode(dst);

  // explicit end
  dst = bcode_copy_def(src, 0);
  CuAssertPtrNotNull(tc, dst);
  CuAssertIntEquals(tc, 3, dst->allocated);
  CuAssertIntEquals(tc, 3, dst->used);
  CuAssertPtrNotNull(tc, dst->inst);
  CuAssertIntEquals(tc, B_ADD, dst->inst[0].op);
  CuAssertIntEquals(tc, B_SUB, dst->inst[1].op);
  CuAssertIntEquals(tc, B_END_DEF, dst->inst[2].op);
  delete_bcode(dst);

  delete_bcode(src);
}

static void test_bcode_index(CuTest* tc) {
  SOURCE* src = new_source(NULL);
  BCODE* bc = NULL;
  LINE_MAP* idx = NULL;
  unsigned pos;
  bool found;

  enter_source_line(src, 10, "LET A=3");
  enter_source_line(src, 20, "LET B=7");
  enter_source_line(src, 30, "PRINT A+B");
  enter_source_line(src, 40, "END");

  // Empty bcode
  bc = new_bcode();
  idx = bcode_index(bc, src);
  CuAssertPtrNotNull(tc, idx);
  found = lookup_line_mapping(idx, 10, &pos);
  CuAssertIntEquals(tc, false, found);
  delete_line_map(idx);

  // Compiled bcode
  emit_source_line(bc, B_SOURCE_LINE, 0);
  emit_num(bc, B_PUSH_NUM, 3);
  emit_var(bc, B_SET_SIMPLE_NUM, 0);
  emit_source_line(bc, B_SOURCE_LINE, 1);
  emit_num(bc, B_PUSH_NUM, 7);
  emit_var(bc, B_SET_SIMPLE_NUM, 1);
  emit_source_line(bc, B_SOURCE_LINE, 2);
  // omit PRINT code to test consecutive LINE
  emit_source_line(bc, B_SOURCE_LINE, 3);
  emit(bc, B_END);
  idx = bcode_index(bc, src);

  found = lookup_line_mapping(idx, 10, &pos);
  CuAssertIntEquals(tc, true, found);
  CuAssertIntEquals(tc, 0, pos);

  found = lookup_line_mapping(idx, 20, &pos);
  CuAssertIntEquals(tc, true, found);
  CuAssertIntEquals(tc, 3, pos);

  found = lookup_line_mapping(idx, 30, &pos);
  CuAssertIntEquals(tc, true, found);
  CuAssertIntEquals(tc, 6, pos);

  found = lookup_line_mapping(idx, 40, &pos);
  CuAssertIntEquals(tc, true, found);
  CuAssertIntEquals(tc, 7, pos);

  found = lookup_line_mapping(idx, 25, &pos);
  CuAssertIntEquals(tc, false, found);

  delete_line_map(idx);

  // Clean up
  delete_bcode(bc);
  delete_source(src);
}

CuSuite* bcode_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_bcode);
  SUITE_ADD_TEST(suite, test_def_end);
  SUITE_ADD_TEST(suite, test_bcode_copy_def);
  SUITE_ADD_TEST(suite, test_bcode_index);
  return suite;
}

#endif
