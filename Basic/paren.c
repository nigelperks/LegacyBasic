// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks
// Utility functions for handling parenthesised symbols (array, DEF, builtin):
// A$(), FNA(), CHR$()

#include <assert.h>
#include "paren.h"
#include "arrays.h"
#include "def.h"
#include "builtin.h"
#include "stringlist.h"
#include "utils.h"

const char* paren_kind(int k) {
  static const char* names[] = {
    "array",
    "user-defined function",
    "built-in function"
  };

  assert(k >= 0 && k < sizeof names / sizeof names[0]);

  return names[k];
}

static void deinit_paren_symbol(PAREN_SYMBOL* sym) {
  assert(sym != NULL);
  switch (sym->kind) {
    case PK_ARRAY:
      switch (sym->type) {
        case TYPE_NUM:
          delete_numeric_array(sym->u.numarr);
          sym->u.numarr = NULL;
          break;
        case TYPE_STR:
          delete_string_array(sym->u.strarr);
          sym->u.strarr = NULL;
          break;
      }
      break;
    case PK_DEF:
      delete_def(sym->u.def);
      sym->u.def = NULL;
      break;
  }
}

void init_paren_symbols(PAREN_SYMBOLS* p) {
  p->sym = NULL;
  p->allocated = 0;
  p->used = 0;
}

void deinit_paren_symbols(PAREN_SYMBOLS* p) {
  assert(p != NULL);
  for (unsigned i = 0; i < p->used; i++)
    deinit_paren_symbol(p->sym + i);
  efree(p->sym);
  p->sym = NULL;
  p->allocated = 0;
  p->used = 0;
}

PAREN_SYMBOL* lookup_paren_name(const PAREN_SYMBOLS* p, unsigned name) {
  for (unsigned i = 0; i < p->used; i++) {
    if (p->sym[i].name == name)
      return &p->sym[i];
  }

  return NULL;
}

static PAREN_SYMBOL* insert_paren_symbol(PAREN_SYMBOLS* p, unsigned name, int type, int kind) {
  assert(p->used <= p->allocated);
  if (p->used == p->allocated) {
    p->allocated = p->allocated ? 2 * p->allocated : 16;
    p->sym = erealloc(p->sym, p->allocated * sizeof p->sym[0]);
  }
  assert(p->used < p->allocated);
  PAREN_SYMBOL* sym = p->sym + p->used++;
  sym->name = name;
  sym->type = type;
  sym->kind = kind;
  return sym;
}

PAREN_SYMBOL* insert_numeric_array(PAREN_SYMBOLS* p, unsigned name, unsigned base, unsigned dimensions, unsigned max[]) {
  struct numeric_array * arr = new_numeric_array(base, dimensions, max);
  if (arr == NULL)
    return NULL;

  PAREN_SYMBOL* sym = insert_paren_symbol(p, name, TYPE_NUM, PK_ARRAY);
  sym->u.numarr = arr;
  return sym;
}

bool replace_numeric_array(PAREN_SYMBOL* sym, unsigned base, unsigned dimensions, unsigned max[]) {
  struct numeric_array * arr = new_numeric_array(base, dimensions, max);
  if (arr == NULL)
    return false;

  delete_numeric_array(sym->u.numarr);
  sym->u.numarr = arr;
  return true;
}

PAREN_SYMBOL* insert_string_array(PAREN_SYMBOLS* p, unsigned name, unsigned base, unsigned dimensions, unsigned max[]) {
  struct string_array * arr = new_string_array(base, dimensions, max);
  if (arr == NULL)
    return NULL;

  PAREN_SYMBOL* sym = insert_paren_symbol(p, name, TYPE_STR, PK_ARRAY);
  sym->u.strarr = arr;
  return sym;
}

bool replace_string_array(PAREN_SYMBOL* sym, unsigned base, unsigned dimensions, unsigned max[]) {
  struct string_array * arr = new_string_array(base, dimensions, max);
  if (arr == NULL)
    return false;

  delete_string_array(sym->u.strarr);
  sym->u.strarr = arr;
  return true;
}

PAREN_SYMBOL* insert_def(PAREN_SYMBOLS* p, unsigned name, int type, unsigned params, unsigned source_line, const BCODE* bcode, unsigned pc) {
  PAREN_SYMBOL* sym = insert_paren_symbol(p, name, type, PK_DEF);
  sym->u.def = new_def();
  return replace_def(sym, params, source_line, bcode, pc) ? sym : NULL;
}

bool replace_def(PAREN_SYMBOL* sym, unsigned params, unsigned source_line, const BCODE* bcode, unsigned pc) {
  BCODE* code = bcode_copy_def(bcode, pc);
  if (code == NULL)
    return false;

  struct def * def = sym->u.def;
  def->params = params;
  def->source_line = source_line;
  delete_bcode(def->code);
  def->code = code;
  return true;
}

static void insert_builtin(PAREN_SYMBOLS* p, const BUILTIN* b, STRINGLIST* names) {
  insert_paren_symbol(p, name_entry(names, b->name), b->type, PK_BUILTIN);
}

void insert_builtins(PAREN_SYMBOLS* p, STRINGLIST* names) {
  const BUILTIN* b;

  for (unsigned i = 0; b = builtin_number(i); i++)
    insert_builtin(p, b, names);
}

#ifdef UNIT_TEST

#include <string.h>
#include "CuTest.h"
#include "emit.h"

static void test_paren_kind(CuTest* tc) {
  CuAssertStrEquals(tc, "array", paren_kind(PK_ARRAY));
  CuAssertStrEquals(tc, "built-in function", paren_kind(PK_BUILTIN));
  CuAssertStrEquals(tc, "user-defined function", paren_kind(PK_DEF));
}

static void test_deinit_paren_symbol(CuTest* tc) {
  PAREN_SYMBOL sym;
  unsigned char buf[sizeof sym.u];

  sym.kind = PK_BUILTIN;
  sym.type = TYPE_NUM;
  memset(&sym.u, 0xff, sizeof sym.u);
  deinit_paren_symbol(&sym);
  CuAssertIntEquals(tc, PK_BUILTIN, sym.kind);
  memset(buf, 0xff, sizeof buf);
  CuAssertTrue(tc, memcmp(&sym.u, buf, sizeof buf) == 0);

  sym.kind = PK_ARRAY;
  sym.type = TYPE_NUM;
  unsigned max[2];
  max[0] = 3;
  max[1] = 4;
  sym.u.numarr = new_numeric_array(1, 2, max);
  deinit_paren_symbol(&sym);
  CuAssertIntEquals(tc, PK_ARRAY, sym.kind);
  CuAssertPtrEquals(tc, NULL, sym.u.numarr);

  sym.kind = PK_ARRAY;
  sym.type = TYPE_STR;
  max[0] = 7;
  sym.u.numarr = new_numeric_array(0, 1, max);
  deinit_paren_symbol(&sym);
  CuAssertIntEquals(tc, PK_ARRAY, sym.kind);
  CuAssertPtrEquals(tc, NULL, sym.u.strarr);

  sym.kind = PK_DEF;
  sym.type = TYPE_NUM;
  sym.u.def = new_def();
  deinit_paren_symbol(&sym);
  CuAssertIntEquals(tc, PK_DEF, sym.kind);
  CuAssertPtrEquals(tc, NULL, sym.u.def);
}

static void test_paren_symbols(CuTest* tc) {
  PAREN_SYMBOLS ps;
  PAREN_SYMBOL* sym;
  unsigned max[2];
  bool succ;

  // Initialise
  memset(&ps, 0xff, sizeof ps);
  init_paren_symbols(&ps);
  CuAssertPtrEquals(tc, NULL, ps.sym);
  CuAssertIntEquals(tc, 0, ps.allocated);
  CuAssertIntEquals(tc, 0, ps.used);

  // Lookup non-existent name
  sym = lookup_paren_name(&ps, 0);
  CuAssertPtrEquals(tc, NULL, sym);

  // Add builtin using generic insert
  sym = insert_paren_symbol(&ps, 3, TYPE_NUM, PK_BUILTIN);
  CuAssertPtrNotNull(tc, sym);
  CuAssertPtrNotNull(tc, ps.sym);
  CuAssertTrue(tc, ps.allocated >= 1);
  CuAssertIntEquals(tc, 1, ps.used);
  CuAssertPtrEquals(tc, ps.sym, sym);
  CuAssertIntEquals(tc, 3, sym->name);
  CuAssertIntEquals(tc, TYPE_NUM, sym->type);
  CuAssertIntEquals(tc, PK_BUILTIN, sym->kind);

  sym = lookup_paren_name(&ps, 3);
  CuAssertPtrEquals(tc, ps.sym, sym);

  // Add numeric array
  max[0] = 3;
  max[1] = 4;
  sym = insert_numeric_array(&ps, 5, 1, 2, max);
  CuAssertPtrNotNull(tc, sym);
  CuAssertTrue(tc, ps.allocated >= 2);
  CuAssertIntEquals(tc, 2, ps.used);
  CuAssertPtrEquals(tc, ps.sym + 1, sym);
  CuAssertIntEquals(tc, 5, sym->name);
  CuAssertIntEquals(tc, TYPE_NUM, sym->type);
  CuAssertIntEquals(tc, PK_ARRAY, sym->kind);
  CuAssertPtrNotNull(tc, sym->u.numarr);
  CuAssertIntEquals(tc, 1, sym->u.numarr->size.base);
  CuAssertIntEquals(tc, 2, sym->u.numarr->size.dimensions);
  CuAssertIntEquals(tc, 3, sym->u.numarr->size.max[0]);
  CuAssertIntEquals(tc, 4, sym->u.numarr->size.max[1]);
  CuAssertIntEquals(tc, 12, sym->u.numarr->size.elements);
  sym->u.numarr->val[0] = 3.1416;
  sym->u.numarr->val[11] = -2.72;

  sym = lookup_paren_name(&ps, 3);
  CuAssertPtrEquals(tc, ps.sym, sym);

  sym = lookup_paren_name(&ps, 5);
  CuAssertPtrEquals(tc, ps.sym + 1, sym);

  // Add string array
  max[0] = 7;
  sym = insert_string_array(&ps, 1, 0, 1, max);
  CuAssertPtrNotNull(tc, sym);
  CuAssertTrue(tc, ps.allocated >= 3);
  CuAssertIntEquals(tc, 3, ps.used);
  CuAssertPtrEquals(tc, ps.sym + 2, sym);
  CuAssertIntEquals(tc, 1, sym->name);
  CuAssertIntEquals(tc, TYPE_STR, sym->type);
  CuAssertIntEquals(tc, PK_ARRAY, sym->kind);
  CuAssertPtrNotNull(tc, sym->u.strarr);
  CuAssertIntEquals(tc, 0, sym->u.strarr->size.base);
  CuAssertIntEquals(tc, 1, sym->u.strarr->size.dimensions);
  CuAssertIntEquals(tc, 7, sym->u.strarr->size.max[0]);
  CuAssertIntEquals(tc, 8, sym->u.strarr->size.elements);
  sym->u.strarr->val[0] = estrdup("HELLO");
  sym->u.strarr->val[7] = estrdup("GOODBYE");

  sym = lookup_paren_name(&ps, 3);
  CuAssertPtrEquals(tc, ps.sym, sym);

  sym = lookup_paren_name(&ps, 1);
  CuAssertPtrEquals(tc, ps.sym + 2, sym);

  // Insert user-defined function
  BCODE* bc = new_bcode();
  emit(bc, B_ADD);
  emit(bc, B_SUB);
  emit(bc, B_MUL);
  emit(bc, B_END_DEF);
  sym = insert_def(&ps, 10, TYPE_NUM, /*params*/ 2, /*line*/ 3, bc, /*pc*/ 1);
  CuAssertPtrNotNull(tc, sym);
  CuAssertTrue(tc, ps.allocated >= 4);
  CuAssertIntEquals(tc, 4, ps.used);
  CuAssertPtrEquals(tc, ps.sym + 3, sym);
  CuAssertIntEquals(tc, 10, sym->name);
  CuAssertIntEquals(tc, TYPE_NUM, sym->type);
  CuAssertIntEquals(tc, PK_DEF, sym->kind);
  struct def * def = sym->u.def;
  CuAssertPtrNotNull(tc, def);
  CuAssertIntEquals(tc, 2, def->params);
  CuAssertIntEquals(tc, 3, def->source_line);
  CuAssertPtrNotNull(tc, def->code);
  CuAssertTrue(tc, def->code != bc);
  CuAssertIntEquals(tc, 3, def->code->allocated);
  CuAssertIntEquals(tc, 3, def->code->used);
  CuAssertPtrNotNull(tc, def->code->inst);
  CuAssertIntEquals(tc, B_SUB, def->code->inst[0].op);
  CuAssertIntEquals(tc, B_MUL, def->code->inst[1].op);
  CuAssertIntEquals(tc, B_END_DEF, def->code->inst[2].op);

  // Redimension numeric array
  sym = &ps.sym[1];
  max[0] = 8;
  succ = replace_numeric_array(sym, 4, 1, max);
  CuAssertIntEquals(tc, true, succ);
  CuAssertIntEquals(tc, 5, sym->name);
  CuAssertIntEquals(tc, TYPE_NUM, sym->type);
  CuAssertIntEquals(tc, PK_ARRAY, sym->kind);
  CuAssertPtrNotNull(tc, sym->u.numarr);
  CuAssertIntEquals(tc, 4, sym->u.numarr->size.base);
  CuAssertIntEquals(tc, 1, sym->u.numarr->size.dimensions);
  CuAssertIntEquals(tc, 8, sym->u.numarr->size.max[0]);
  CuAssertIntEquals(tc, 5, sym->u.numarr->size.elements);

  // Redimension string array
  sym = &ps.sym[2];
  max[0] = 2;
  max[1] = 2;
  succ = replace_string_array(sym, 2, 2, max);
  CuAssertIntEquals(tc, true, succ);
  CuAssertIntEquals(tc, 1, sym->name);
  CuAssertIntEquals(tc, TYPE_STR, sym->type);
  CuAssertIntEquals(tc, PK_ARRAY, sym->kind);
  CuAssertPtrNotNull(tc, sym->u.strarr);
  CuAssertIntEquals(tc, 2, sym->u.strarr->size.base);
  CuAssertIntEquals(tc, 2, sym->u.strarr->size.dimensions);
  CuAssertIntEquals(tc, 2, sym->u.strarr->size.max[0]);
  CuAssertIntEquals(tc, 2, sym->u.strarr->size.max[1]);
  CuAssertIntEquals(tc, 1, sym->u.strarr->size.elements);

  // Redefine function to empty
  sym = &ps.sym[3];
  replace_def(sym, /*params*/ 4, /*line*/ 9, bc, /*pc*/ 3);
  CuAssertIntEquals(tc, 10, sym->name);
  CuAssertIntEquals(tc, TYPE_NUM, sym->type);
  CuAssertIntEquals(tc, PK_DEF, sym->kind);
  def = sym->u.def;
  CuAssertPtrNotNull(tc, def);
  CuAssertIntEquals(tc, 4, def->params);
  CuAssertIntEquals(tc, 9, def->source_line);
  CuAssertPtrNotNull(tc, def->code);
  CuAssertIntEquals(tc, 1, def->code->allocated);
  CuAssertIntEquals(tc, 1, def->code->used);
  CuAssertPtrNotNull(tc, def->code->inst);
  CuAssertIntEquals(tc, B_END_DEF, def->code->inst[0].op);

  // Clean up
  delete_bcode(bc);
  deinit_paren_symbols(&ps);
}

static void test_insert_builtins(CuTest* tc) {
  PAREN_SYMBOLS ps;
  STRINGLIST* names = new_stringlist();

  init_paren_symbols(&ps);

  insert_builtins(&ps, names);

  CuAssertIntEquals(tc, 26, ps.used); // number of elements of builtins[] in builtin.c

  CuAssertIntEquals(tc, 0, ps.sym[0].name);
  CuAssertStrEquals(tc, "ABS", stringlist_item(names, 0));
  CuAssertIntEquals(tc, PK_BUILTIN, ps.sym[0].kind);
  CuAssertIntEquals(tc, TYPE_NUM, ps.sym[0].type);

  CuAssertIntEquals(tc, 3, ps.sym[3].name);
  CuAssertStrEquals(tc, "CHR$", stringlist_item(names, 3));
  CuAssertIntEquals(tc, PK_BUILTIN, ps.sym[3].kind);
  CuAssertIntEquals(tc, TYPE_STR, ps.sym[3].type);

  CuAssertIntEquals(tc, 25, ps.sym[25].name);
  CuAssertStrEquals(tc, "VAL", stringlist_item(names, 25));
  CuAssertIntEquals(tc, PK_BUILTIN, ps.sym[25].kind);
  CuAssertIntEquals(tc, TYPE_NUM, ps.sym[25].type);

  deinit_paren_symbols(&ps);
  delete_stringlist(names);
}

CuSuite* paren_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_paren_kind);
  SUITE_ADD_TEST(suite, test_deinit_paren_symbol);
  SUITE_ADD_TEST(suite, test_paren_symbols);
  SUITE_ADD_TEST(suite, test_insert_builtins);
  return suite;
}

#endif
