// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#include <stdlib.h>
#include <assert.h>
#include "bcode.h"
#include "utils.h"

static const struct {
  char* name;
  int format;
} ops[] = {
  { "NOP", BF_IMPLICIT },
  // source
  { "LINE", BF_SOURCE_LINE },
  // number
  { "PUSH-NUM", BF_NUM },
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
  { "ON-LINE", BF_BASIC_LINE },
  { "IF-THEN", BF_IMPLICIT },
  // output
  { "PRINT-LN", BF_IMPLICIT },
  { "PRINT-SPC", BF_IMPLICIT },
  { "PRINT-TAB", BF_IMPLICIT },
  { "PRINT-COMMA", BF_IMPLICIT },
  { "PRINT-NUM", BF_IMPLICIT },
  { "PRINT-STR", BF_IMPLICIT },
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
  // builtins
  { "ABS", BF_IMPLICIT },
  { "ASC", BF_IMPLICIT },
  { "ATN", BF_IMPLICIT },
  { "CHR", BF_IMPLICIT },
  { "COS", BF_IMPLICIT },
  { "EXP", BF_IMPLICIT },
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
  { "VAL", BF_IMPLICIT },
};

BCODE* new_bcode(const SOURCE* source) {
  BCODE* p = emalloc(sizeof *p);
  p->source = source;
  p->inst = NULL;
  p->allocated = 0;
  p->used = 0;
  init_stringlist(&p->names);
  return p;
}

void delete_bcode(BCODE* p) {
  if (p) {
    deinit_stringlist(&p->names);
    for (unsigned i = 0; i < p->used; i++) {
      if (p->inst[i].op < sizeof ops / sizeof ops[0] && ops[p->inst[i].op].format == BF_STR)
        efree(p->inst[i].u.str);
    }
    efree(p->inst);
    efree(p);
  }
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
  return i;
}

void print_bcode(const BCODE* p, FILE* fp) {
  assert(p != NULL);
  assert(fp != NULL);
  for (unsigned i = 0; i < p->used; i++)
    print_binst(p, i, fp);
}

void print_binst(const BCODE* p, unsigned j, FILE* fp) {
  assert(p != NULL);
  assert(j < p->used);
  assert(fp != NULL);
  const BINST* i = p->inst + j;
  assert(i->op < sizeof ops / sizeof ops[0]);
  fprintf(fp, "%5u %s ", j, ops[i->op].name);
  switch (ops[i->op].format) {
    case BF_IMPLICIT:
      break;
    case BF_SOURCE_LINE:
      fprintf(fp, "%u: %u %s", i->u.line, source_linenum(p->source, i->u.line), source_text(p->source, i->u.line));
      break;
    case BF_BASIC_LINE:
      fprintf(fp, "%u", i->u.line);
      break;
    case BF_NUM:
      fprintf(fp, "%g", i->u.num);
      break;
    case BF_STR:
      fprintf(fp, "\"%s\"", i->u.str);
      break;
    case BF_VAR:
      fputs(stringlist_item(&p->names, i->u.name), fp);
      break;
    case BF_PARAM:
      fprintf(fp, "%s, %u", stringlist_item(&p->names, i->u.param.name), i->u.param.params);
      break;
    case BF_COUNT:
      fprintf(fp, "%u", i->u.count);
      break;
    default:
      fatal("internal error: print_binst: unknown instruction format: %u\n", (unsigned) ops[i->op].format);
  }
  putc('\n', fp);
}

unsigned bcode_name_entry(BCODE* p, const char* name) {
  return stringlist_entry_case_insensitive(&p->names, name);
}

const char* bcode_name(const BCODE* p, unsigned i) {
  return stringlist_item(&p->names, i);
}

bool bcode_find_basic_line(const BCODE* p, unsigned basic_line, unsigned *source_line) {
  assert(p != NULL);
  for (unsigned i = 0; i < p->used; i++) {
    if (p->inst[i].op == B_LINE) {
      if (source_linenum(p->source, p->inst[i].u.line) == basic_line) {
        *source_line = i;
        return true;
      }
    }
  }
  return false;
}

#ifdef UNIT_TEST

#include "CuTest.h"

static void test_bcode(CuTest* tc) {
  static const char CODE[] = "10 PRINT\n20 PRINT\n";
  SOURCE* const src = load_source_string(CODE, "test");
  BCODE* p;
  BINST* i;
  unsigned j;
  bool succ;

  p = new_bcode(src);
  CuAssertPtrNotNull(tc, p);
  CuAssertTrue(tc, p->source == src);
  CuAssertPtrEquals(tc, NULL, p->inst);
  CuAssertIntEquals(tc, 0, p->allocated);
  CuAssertIntEquals(tc, 0, p->used);
  CuAssertIntEquals(tc, 0, stringlist_count(&p->names));

  i = bcode_next(p, B_ADD);
  CuAssertPtrNotNull(tc, i);
  CuAssertPtrNotNull(tc, p->inst);
  CuAssertTrue(tc, p->allocated >= 1);
  CuAssertIntEquals(tc, 1, p->used);
  CuAssertIntEquals(tc, B_ADD, p->inst[0].op);

  j = bcode_name_entry(p, "Cab$");
  CuAssertIntEquals(tc, 0, j);
  CuAssertIntEquals(tc, 1, stringlist_count(&p->names));
  CuAssertStrEquals(tc, "Cab$", stringlist_item(&p->names, 0));
  CuAssertStrEquals(tc, "Cab$", bcode_name(p, 0));

  j = bcode_name_entry(p, "Lurg");
  CuAssertIntEquals(tc, 1, j);
  CuAssertIntEquals(tc, 2, stringlist_count(&p->names));
  CuAssertStrEquals(tc, "Lurg", stringlist_item(&p->names, 1));
  CuAssertStrEquals(tc, "Lurg", bcode_name(p, 1));

  j = bcode_name_entry(p, "CAB$");
  CuAssertIntEquals(tc, 0, j);

  j = bcode_name_entry(p, "cab$");
  CuAssertIntEquals(tc, 0, j);

  succ = bcode_find_basic_line(p, 1000, &j);
  CuAssertIntEquals(tc, false, succ);

  i = bcode_next(p, B_LINE);
  CuAssertPtrNotNull(tc, i);
  CuAssertIntEquals(tc, 2, p->used);
  i->u.line = 1;
  succ = bcode_find_basic_line(p, 20, &j);
  CuAssertIntEquals(tc, true, succ);
  CuAssertIntEquals(tc, 1, j);

  delete_bcode(p);
  delete_source(src);
}

CuSuite* bcode_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_bcode);
  return suite;
}


#endif
