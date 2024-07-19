// Legacy BASIC
// Copyright (c) 2024 Nigel Perks
// Symbol table used for both compiling and running.

#include <assert.h>
#include "symbol.h"
#include "utils.h"
#include "os.h"

const char* symbol_kind(int kind) {
  switch (kind) {
    case SYM_UNKNOWN: return "unknown: used before defined";
    case SYM_VARIABLE: return "variable";
    case SYM_ARRAY: return "array";
    case SYM_DEF: return "user-defined function";
    case SYM_BUILTIN: return "built-in function";
  }
  return "unknown kind of symbol";
}

SYMTAB* new_symbol_table(void) {
  return ecalloc(1, sizeof (SYMTAB));
}

void delete_symbol_table(SYMTAB* st) {
  if (st) {
    clear_symbol_table_names(st);
    efree(st->psym);
    efree(st);
  }
}

void clear_symbol_table_names(SYMTAB* st) {
  clear_symbol_table_values(st);
  for (unsigned i = 0; i < st->used; i++) {
    efree(st->psym[i]->name);
    efree(st->psym[i]);
  }
  st->used = 0;
  st->next_id = 0;
}

static void undefine_value(SYMBOL*);

// clear values/definitions but keep names so that bcode referencing them remains valid
void clear_symbol_table_values(SYMTAB* st) {
  for (unsigned i = 0; i < st->used; i++) {
    SYMBOL* sym = st->psym[i];
    if (sym->defined)
      undefine_value(sym);
  }
}

static void undefine_value(SYMBOL* sym) {
  if (sym->kind == SYM_BUILTIN)
    // not a value, should not be erased or marked undefined
    return;

  switch (sym->kind) {
    case SYM_VARIABLE:
      switch (sym->type) {
        case TYPE_NUM:
          sym->val.num = 0;
          break;
        case TYPE_STR:
          efree(sym->val.str);
          sym->val.str = NULL;
          break;
      }
      break;
    case SYM_ARRAY:
      switch (sym->type) {
        case TYPE_NUM:
          delete_numeric_array(sym->val.numarr);
          sym->val.numarr = NULL;
          break;
        case TYPE_STR:
          delete_string_array(sym->val.strarr);
          sym->val.strarr = NULL;
          break;
      }
      break;
    case SYM_DEF:
      delete_def(sym->val.def);
      sym->val.def = NULL;
      break;
  }

  sym->defined = false;
}

static bool match_paren(int kind, bool paren) {
  switch (kind) {
    case SYM_UNKNOWN: return paren;
    case SYM_VARIABLE: return !paren;
    case SYM_ARRAY: return paren;
    case SYM_DEF: return paren;
    case SYM_BUILTIN: return true;
  }
  assert(0 && "match_paren: unknown symbol kind");
  return false;
}

SYMBOL* sym_lookup(SYMTAB* st, const char* name, bool paren) {
  for (unsigned i = 0; i < st->used; i++) {
    SYMBOL* sym = st->psym[i];
    if (match_paren(sym->kind, paren) && STRICMP(sym->name, name) == 0)
      return sym;
  }
  return NULL;
}

SYMBOL* sym_insert(SYMTAB* st, const char* name, int kind, int type) {
  assert(st->used <= st->allocated);
  if (st->used == st->allocated) {
    st->allocated = st->allocated ? 2 * st->allocated : 64;
    st->psym = erealloc(st->psym, st->allocated * sizeof st->psym[0]);
  }
  assert(st->used < st->allocated);
  SYMBOL* sym = st->psym[st->used++] = ecalloc(1, sizeof *sym);
  sym->name = estrdup(name);
  sym->id = st->next_id++;
  sym->kind = kind;
  sym->type = type;
  sym->defined = false;
  return sym;
}

SYMBOL* sym_insert_builtin(SYMTAB* st, const char* name, int type, const char* args, int opcode) {
  SYMBOL* sym = sym_insert(st, name, SYM_BUILTIN, type);
  sym->val.builtin.args = args;
  sym->val.builtin.opcode = opcode;
  sym->defined = true;
  return sym;
}

SYMBOL* symbol(SYMTAB* st, SYMID id) {
  assert(st != NULL);
  assert(id < st->used);
  return st->psym[id];
}

const char* sym_name(const SYMTAB* st, SYMID id) {
  assert(st != NULL && id < st->used);
  return st->psym[id]->name;
}

// If a symbol remains of UNKNOWN kind after parsing,
// it has been used with parens but never defined with DIM or DEF,
// so should be treated as an undeclared array.
void sym_make_unknown_array(SYMTAB* st) {
  for (unsigned i = 0; i < st->used; i++) {
    if (st->psym[i]->kind == SYM_UNKNOWN)
      st->psym[i]->kind = SYM_ARRAY;
  }
}

void print_binst(const BINST* i, unsigned j, const SOURCE* source, const SYMTAB* st, FILE* fp) {
  assert(fp != NULL);
  fprintf(fp, "%5u %s ", j, bcode_name(i->op));
  int fmt = bcode_format(i->op);
  switch (fmt) {
    case BF_IMPLICIT:
      break;
    case BF_SOURCE_LINE:
      fprintf(fp, "%u", i->u.source_line);
      if (source)
        fprintf(fp, ": %u %s", source_linenum(source, i->u.source_line), source_text(source, i->u.source_line));
      break;
    case BF_BASIC_LINE:
      fprintf(fp, "%u", i->u.basic_line.lineno);
      break;
    case BF_NUM:
      fprintf(fp, "%g", i->u.num);
      break;
    case BF_STR:
      if (i->u.str)
        fprintf(fp, "\"%s\"", i->u.str);
      else
        fputs("null", fp);
      break;
    case BF_VAR:
      fputs(sym_name(st, i->u.symbol_id), fp);
      break;
    case BF_PARAM:
      fprintf(fp, "%s, %u", sym_name(st, i->u.param.symbol_id), i->u.param.params);
      break;
    case BF_COUNT:
      fprintf(fp, "%u", i->u.count);
      break;
    default:
      fatal("internal error: print_binst: unknown instruction format: %d\n", fmt);
  }
  putc('\n', fp);
}

#ifdef UNIT_TEST

#include "CuTest.h"

static void test_symbol_kind(CuTest* tc) {
  CuAssertStrEquals(tc, "variable", symbol_kind(SYM_VARIABLE));
  CuAssertStrEquals(tc, "built-in function", symbol_kind(SYM_BUILTIN));
  CuAssertStrEquals(tc, "unknown kind of symbol", symbol_kind(SYM_BUILTIN + 1));
}

static void test_match_paren(CuTest* tc) {
  // can a non-parenthesised symbol use match a VARIABLE entry?
  CuAssertIntEquals(tc, true, match_paren(SYM_VARIABLE, false));
  // can a parenthesised symbol use match a VARIABLE entry?
  CuAssertIntEquals(tc, false, match_paren(SYM_VARIABLE, true));
  // can a non-parenthesised symbol use match an ARRAY entry?
  CuAssertIntEquals(tc, false, match_paren(SYM_ARRAY, false));
  // can a parenthesised symbol use match an ARRAY entry?
  CuAssertIntEquals(tc, true, match_paren(SYM_ARRAY, true));
  // can a non-parenthesised symbol use match a built-in function entry?
  CuAssertIntEquals(tc, true, match_paren(SYM_BUILTIN, false));
  // can a parenthesised symbol use match a built-in function entry?
  CuAssertIntEquals(tc, true, match_paren(SYM_BUILTIN, true));
}

static void test_new_symbol_table(CuTest* tc) {
  SYMTAB* st = new_symbol_table();
  CuAssertPtrEquals(tc, NULL, st->psym);
  CuAssertIntEquals(tc, 0, st->allocated);
  CuAssertIntEquals(tc, 0, st->used);
  CuAssertIntEquals(tc, 0, st->next_id);
  delete_symbol_table(st);
}

static void test_insert(CuTest* tc) {
  SYMTAB* st = new_symbol_table();

  SYMBOL* apple = sym_insert(st, "apple", SYM_VARIABLE, TYPE_NUM);
  CuAssertPtrNotNull(tc, st->psym);
  CuAssertIntEquals(tc, 64, st->allocated);
  CuAssertIntEquals(tc, 1, st->used);
  CuAssertIntEquals(tc, 1, st->next_id);
  CuAssertPtrNotNull(tc, apple);
  CuAssertStrEquals(tc, "apple", apple->name);
  CuAssertIntEquals(tc, 0, apple->id);
  CuAssertIntEquals(tc, SYM_VARIABLE, apple->kind);
  CuAssertIntEquals(tc, TYPE_NUM, apple->type);
  CuAssertIntEquals(tc, false, apple->defined);

  SYMBOL* banana = sym_insert(st, "banana", SYM_ARRAY, TYPE_STR);
  CuAssertPtrNotNull(tc, st->psym);
  CuAssertIntEquals(tc, 64, st->allocated);
  CuAssertIntEquals(tc, 2, st->used);
  CuAssertIntEquals(tc, 2, st->next_id);
  CuAssertPtrNotNull(tc, banana);
  CuAssertStrEquals(tc, "banana", banana->name);
  CuAssertIntEquals(tc, 1, banana->id);
  CuAssertIntEquals(tc, SYM_ARRAY, banana->kind);
  CuAssertIntEquals(tc, TYPE_STR, banana->type);
  CuAssertIntEquals(tc, false, banana->defined);

  SYMBOL* rnd = sym_insert_builtin(st, "RND", TYPE_NUM, "d", B_RND);
  CuAssertPtrNotNull(tc, st->psym);
  CuAssertIntEquals(tc, 64, st->allocated);
  CuAssertIntEquals(tc, 3, st->used);
  CuAssertIntEquals(tc, 3, st->next_id);
  CuAssertPtrNotNull(tc, rnd);
  CuAssertStrEquals(tc, "RND", rnd->name);
  CuAssertIntEquals(tc, 2, rnd->id);
  CuAssertIntEquals(tc, SYM_BUILTIN, rnd->kind);
  CuAssertIntEquals(tc, TYPE_NUM, rnd->type);
  CuAssertIntEquals(tc, true, rnd->defined);
  CuAssertStrEquals(tc, "d", rnd->val.builtin.args);
  CuAssertIntEquals(tc, B_RND, rnd->val.builtin.opcode);

  CuAssertPtrEquals(tc, apple, symbol(st, 0));
  CuAssertPtrEquals(tc, banana, symbol(st, 1));
  CuAssertPtrEquals(tc, rnd, symbol(st, 2));

  CuAssertStrEquals(tc, "banana", sym_name(st, 1));

  delete_symbol_table(st);
}

static void test_lookup(CuTest* tc) {
  SYMTAB* st = new_symbol_table();
  SYMBOL* sym;

  // Non-existent name
  sym = sym_lookup(st, "name$", false);
  CuAssertPtrEquals(tc, NULL, sym);

  sym = sym_lookup(st, "name$", true);
  CuAssertPtrEquals(tc, NULL, sym);

  // Non-parenthesised name
  SYMBOL* name_str = sym_insert(st, "name$", SYM_VARIABLE, TYPE_STR);

  sym = sym_lookup(st, "name$", true);
  CuAssertPtrEquals(tc, NULL, sym);

  sym = sym_lookup(st, "name$", false);
  CuAssertPtrEquals(tc, name_str, sym);
  CuAssertStrEquals(tc, "name$", sym->name);
  CuAssertIntEquals(tc, 0, sym->id);
  CuAssertIntEquals(tc, SYM_VARIABLE, sym->kind);
  CuAssertIntEquals(tc, TYPE_STR, sym->type);
  CuAssertIntEquals(tc, false, sym->defined);

  // Built-in matches both parenthesised and non-parenthesised use: RND, RND(0)
  SYMBOL* rnd = sym_insert_builtin(st, "RND", TYPE_NUM, "d", B_RND);
  sym = sym_lookup(st, "RND", false);
  CuAssertPtrEquals(tc, rnd, sym);
  sym = sym_lookup(st, "RND", true);
  CuAssertPtrEquals(tc, rnd, sym);

  // Case-insensitive
  sym = sym_lookup(st, "NAME$", false);
  CuAssertPtrEquals(tc, name_str, sym);
  sym = sym_lookup(st, "NaMe$", false);
  CuAssertPtrEquals(tc, name_str, sym);

  // UNKNOWN matches parenthesised use only, even though its exact kind is unknown
  sym = sym_insert(st, "XY", SYM_UNKNOWN, TYPE_NUM);
  CuAssertPtrEquals(tc, NULL, sym_lookup(st, "xy", false));
  CuAssertPtrEquals(tc, sym, sym_lookup(st, "xy", true));

  delete_symbol_table(st);
}

static void test_undefine(CuTest* tc) {
  const unsigned dim1[] = { 12 };
  const unsigned dim2[] = { 3, 5 };
  SYMBOL sym;

  sym.kind = SYM_ARRAY;
  sym.type = TYPE_NUM;
  sym.val.numarr = new_numeric_array(0, 2, dim2);
  CuAssertPtrNotNull(tc, sym.val.numarr);
  sym.defined = true;
  undefine_value(&sym);
  CuAssertPtrEquals(tc, NULL, sym.val.numarr);
  CuAssertIntEquals(tc, false, sym.defined);

  sym.kind = SYM_ARRAY;
  sym.type = TYPE_STR;
  sym.val.strarr = new_string_array(1, 1, dim1);
  CuAssertPtrNotNull(tc, sym.val.strarr);
  sym.defined = true;
  undefine_value(&sym);
  CuAssertPtrEquals(tc, NULL, sym.val.strarr);
  CuAssertIntEquals(tc, false, sym.defined);

  BCODE* bc = new_bcode();
  sym.kind = SYM_DEF;
  sym.type = TYPE_NUM;
  sym.val.def = new_def(bc, NULL, 0);
  CuAssertPtrNotNull(tc, sym.val.def);
  sym.defined = true;
  undefine_value(&sym);
  CuAssertPtrEquals(tc, NULL, sym.val.def);
  CuAssertIntEquals(tc, false, sym.defined);

  sym.kind = SYM_BUILTIN;
  sym.type = TYPE_STR;
  sym.val.builtin.args = "snn";
  sym.val.builtin.opcode = B_MID3;
  sym.defined = true;
  undefine_value(&sym);
  CuAssertIntEquals(tc, true, sym.defined);
  CuAssertStrEquals(tc, "snn", sym.val.builtin.args);
  CuAssertIntEquals(tc, B_MID3, sym.val.builtin.opcode);

  sym.kind = SYM_VARIABLE;
  sym.type = TYPE_NUM;
  sym.val.num = 321;
  sym.defined = true;
  undefine_value(&sym);
  CuAssertIntEquals(tc, false, sym.defined);

  sym.kind = SYM_VARIABLE;
  sym.type = TYPE_STR;
  sym.val.str = estrdup("Henry");
  sym.defined = true;
  undefine_value(&sym);
  CuAssertPtrEquals(tc, NULL, sym.val.str);
  CuAssertIntEquals(tc, false, sym.defined);
}

static void test_clear_values(CuTest* tc) {
  SYMTAB* st = new_symbol_table();
  SYMBOL* sym;

  sym = sym_insert(st, "X1", SYM_VARIABLE, TYPE_NUM);
  sym->val.num = 987;
  sym->defined = true;

  sym = sym_insert(st, "X$", SYM_VARIABLE, TYPE_STR);
  sym->val.str = estrdup("Custard");
  sym->defined = true;

  const unsigned dim2[] = { 8, 3 };
  sym = sym_insert(st, "W", SYM_ARRAY, TYPE_NUM);
  sym->val.numarr = new_numeric_array(1, 2, dim2);
  sym->defined = true;

  sym = sym_insert(st, "FNA$", SYM_DEF, TYPE_STR);
  sym->val.def = new_def(new_bcode(), NULL, 0);
  sym->defined = true;

  sym = sym_insert_builtin(st, "TIME$", TYPE_STR, "d", B_TIME_STR);
  CuAssertIntEquals(tc, true, sym->defined);

  clear_symbol_table_values(st);

  sym = sym_lookup(st, "X1", false);
  CuAssertPtrNotNull(tc, sym);
  CuAssertIntEquals(tc, false, sym->defined);
  CuAssertDblEquals(tc, 0, sym->val.num, 0);

  sym = sym_lookup(st, "X$", false);
  CuAssertPtrNotNull(tc, sym);
  CuAssertIntEquals(tc, false, sym->defined);
  CuAssertPtrEquals(tc, NULL, sym->val.str);

  sym = sym_lookup(st, "W", true);
  CuAssertPtrNotNull(tc, sym);
  CuAssertIntEquals(tc, false, sym->defined);
  CuAssertPtrEquals(tc, NULL, sym->val.numarr);

  sym = sym_lookup(st, "FNA$", true);
  CuAssertPtrNotNull(tc, sym);
  CuAssertIntEquals(tc, false, sym->defined);
  CuAssertPtrEquals(tc, NULL, sym->val.def);

  sym = sym_lookup(st, "TIME$", true);
  CuAssertPtrNotNull(tc, sym);
  CuAssertIntEquals(tc, true, sym->defined);

  sym = sym_lookup(st, "TIME$", false);
  CuAssertPtrNotNull(tc, sym);
  CuAssertIntEquals(tc, true, sym->defined);

  delete_symbol_table(st);
}

static void test_clear_names(CuTest* tc) {
  SYMTAB* st = new_symbol_table();
  SYMBOL* sym;

  sym = sym_insert(st, "X1", SYM_VARIABLE, TYPE_NUM);
  sym->val.num = 987;
  sym->defined = true;

  sym = sym_insert(st, "X$", SYM_VARIABLE, TYPE_STR);
  sym->val.str = estrdup("Custard");
  sym->defined = true;

  const unsigned dim2[] = { 8, 3 };
  sym = sym_insert(st, "W", SYM_ARRAY, TYPE_NUM);
  sym->val.numarr = new_numeric_array(1, 2, dim2);
  sym->defined = true;

  sym = sym_insert(st, "FNA$", SYM_DEF, TYPE_STR);
  sym->val.def = new_def(new_bcode(), NULL, 0);
  sym->defined = true;

  sym = sym_insert_builtin(st, "TIME$", TYPE_STR, "d", B_TIME_STR);
  CuAssertIntEquals(tc, true, sym->defined);

  clear_symbol_table_names(st);

  CuAssertPtrEquals(tc, NULL, sym_lookup(st, "X1", false));
  CuAssertPtrEquals(tc, NULL, sym_lookup(st, "X$", false));
  CuAssertPtrEquals(tc, NULL, sym_lookup(st, "W", true));
  CuAssertPtrEquals(tc, NULL, sym_lookup(st, "FNA$", true));
  CuAssertPtrEquals(tc, NULL, sym_lookup(st, "TIME$", true));
  CuAssertPtrEquals(tc, NULL, sym_lookup(st, "TIME$", false));

  CuAssertPtrNotNull(tc, st->psym);
  CuAssertIntEquals(tc, 64, st->allocated);
  CuAssertIntEquals(tc, 0, st->used);
  CuAssertIntEquals(tc, 0, st->next_id);

  sym = sym_insert(st, "NEW", SYM_VARIABLE, TYPE_NUM);
  CuAssertIntEquals(tc, 1, st->used);
  CuAssertIntEquals(tc, 1, st->next_id);

  delete_symbol_table(st);
}

CuSuite* symbol_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_symbol_kind);
  SUITE_ADD_TEST(suite, test_match_paren);
  SUITE_ADD_TEST(suite, test_new_symbol_table);
  SUITE_ADD_TEST(suite, test_insert);
  SUITE_ADD_TEST(suite, test_lookup);
  SUITE_ADD_TEST(suite, test_undefine);
  SUITE_ADD_TEST(suite, test_clear_values);
  SUITE_ADD_TEST(suite, test_clear_names);
  return suite;
}

#endif // UNIT_TEST
