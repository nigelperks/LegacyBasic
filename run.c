// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include "run.h"
#include "builtin.h"
#include "utils.h"

struct numvar {
  unsigned name;
  double val;
};

#define MAX_NUM_VAR (64)

typedef struct {
  struct numvar vars[MAX_NUM_VAR];
  unsigned count;
} NUMVARS;

struct strvar {
  unsigned name;
  char* val;
};

#define MAX_STR_VAR (32)

typedef struct {
  struct strvar vars[MAX_STR_VAR];
  unsigned count;
} STRVARS;

#define MAX_DIMENSIONS (2)

#define MAX_ELEMENTS (64 * 1024)

static bool compute_total_elements(unsigned base, unsigned dimensions, unsigned max[], unsigned *elements) {
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

struct array_size {
  unsigned short base;
  unsigned short dimensions;
  unsigned short max[MAX_DIMENSIONS];
  unsigned elements;
};

static bool init_array_size(struct array_size * p, unsigned base, unsigned dimensions, unsigned max[], unsigned elements) {
  if (dimensions < 1 || dimensions > MAX_DIMENSIONS)
    return false;

  p->base = base;
  p->dimensions = dimensions;
  for (unsigned i = 0; i < dimensions; i++)
    p->max[i] = max[i];
  p->elements = elements;
  return true;
}

static bool compute_element_offset(const struct array_size * p, unsigned dimensions, unsigned indexes[], unsigned *offset) {
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

struct numeric_array {
  struct array_size size;
  double val[0];
};

static void delete_numeric_array(struct numeric_array * p) {
  efree(p);
}

static struct numeric_array * new_numeric_array(unsigned base, unsigned dimensions, unsigned max[]) {
  unsigned elements;
  if (!compute_total_elements(base, dimensions, max, &elements))
    return NULL;
  struct numeric_array * p = ecalloc(1, sizeof *p + elements * sizeof p->val[0]);
  if (!init_array_size(&p->size, base, dimensions, max, elements)) {
    delete_numeric_array(p);
    return NULL;
  }
  return p;
}

static bool compute_numeric_element(struct numeric_array * p, unsigned dimensions, unsigned indexes[], double* *addr) {
  unsigned offset;
  if (compute_element_offset(&p->size, dimensions, indexes, &offset)) {
    *addr = p->val + offset;
    return true;
  }
  return false;
}

struct string_array {
  struct array_size size;
  char* val[0];
};

static void delete_string_array(struct string_array * p) {
  if (p) {
    for (unsigned i = 0; i < p->size.elements; i++)
      efree(p->val[i]);
    efree(p);
  }
}

static struct string_array * new_string_array(unsigned base, unsigned dimensions, unsigned max[]) {
  unsigned elements;
  if (!compute_total_elements(base, dimensions, max, &elements))
    return NULL;
  struct string_array * p = ecalloc(1, sizeof *p + elements * sizeof p->val[0]);
  if (!init_array_size(&p->size, base, dimensions, max, elements)) {
    delete_string_array(p);
    return NULL;
  }
  return p;
}

static bool compute_string_element(struct string_array * p, unsigned dimensions, unsigned indexes[], char* * *addr) {
  unsigned offset;
  if (compute_element_offset(&p->size, dimensions, indexes, &offset)) {
    *addr = p->val + offset;
    return true;
  }
  return false;
}

struct def {
  unsigned params;
  unsigned source_line;
  unsigned pc;
};

static struct def * new_def(unsigned params, unsigned source_line, unsigned pc) {
  struct def * p = emalloc(sizeof *p);
  p->params = params;
  p->source_line = source_line;
  p->pc = pc;
  return p;
}

static void delete_def(struct def * def) {
  efree(def);
}

enum { PK_ARRAY, PK_DEF, PK_BUILTIN };

static const char* paren_kind(int k) {
  static const char* names[] = {
    "array",
    "user-defined function",
    "built-in function"
  };

  assert(k >= 0 && k < sizeof names / sizeof names[0]);

  return names[k];
}

typedef struct {
  unsigned name;
  short type;
  short kind;
  union {
    struct numeric_array * numarr;
    struct string_array * strarr;
    struct def * def;
  } u;
} PAREN_SYMBOL;

static void deinit_paren_symbol(PAREN_SYMBOL* sym) {
  assert(sym != NULL);
  switch (sym->kind) {
    case PK_ARRAY:
      switch (sym->type) {
        case TYPE_NUM: delete_numeric_array(sym->u.numarr); break;
        case TYPE_STR: delete_string_array(sym->u.strarr); break;
      }
      break;
    case PK_DEF:
      delete_def(sym->u.def);
      break;
  }
}

typedef struct {
  PAREN_SYMBOL* sym;
  unsigned allocated;
  unsigned used;
} PAREN_SYMBOLS;

static void init_paren_symbols(PAREN_SYMBOLS* p) {
  p->sym = NULL;
  p->allocated = 0;
  p->used = 0;
}

static void deinit_paren_symbols(PAREN_SYMBOLS* p) {
  assert(p != NULL);
  for (unsigned i = 0; i < p->used; i++)
    deinit_paren_symbol(p->sym + i);
  efree(p->sym);
  p->sym = NULL;
  p->allocated = 0;
  p->used = 0;
}

static PAREN_SYMBOL* lookup_paren_name(const PAREN_SYMBOLS* p, unsigned name) {
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

static PAREN_SYMBOL* insert_numeric_array(PAREN_SYMBOLS* p, unsigned name, unsigned base, unsigned dimensions, unsigned max[]) {
  struct numeric_array * arr = new_numeric_array(base, dimensions, max);
  if (arr == NULL)
    return NULL;

  PAREN_SYMBOL* sym = insert_paren_symbol(p, name, TYPE_NUM, PK_ARRAY);
  sym->u.numarr = arr;
  return sym;
}

static bool replace_numeric_array(PAREN_SYMBOL* sym, unsigned base, unsigned dimensions, unsigned max[]) {
  struct numeric_array * arr = new_numeric_array(base, dimensions, max);
  if (arr == NULL)
    return false;

  delete_numeric_array(sym->u.numarr);
  sym->u.numarr = arr;
  return true;
}

static PAREN_SYMBOL* insert_string_array(PAREN_SYMBOLS* p, unsigned name, unsigned base, unsigned dimensions, unsigned max[]) {
  struct string_array * arr = new_string_array(base, dimensions, max);
  if (arr == NULL)
    return NULL;

  PAREN_SYMBOL* sym = insert_paren_symbol(p, name, TYPE_STR, PK_ARRAY);
  sym->u.strarr = arr;
  return sym;
}

static bool replace_string_array(PAREN_SYMBOL* sym, unsigned base, unsigned dimensions, unsigned max[]) {
  struct string_array * arr = new_string_array(base, dimensions, max);
  if (arr == NULL)
    return false;

  delete_string_array(sym->u.strarr);
  sym->u.strarr = arr;
  return true;
}

static PAREN_SYMBOL* insert_def(PAREN_SYMBOLS* p, unsigned name, int type, unsigned params, unsigned source_line, unsigned pc) {
  PAREN_SYMBOL* sym = insert_paren_symbol(p, name, type, PK_DEF);
  sym->u.def = new_def(params, source_line, pc);
  return sym;
}

static void replace_def(PAREN_SYMBOL* sym, unsigned params, unsigned source_line, unsigned pc) {
  struct def * def = sym->u.def;
  def->params = params;
  def->source_line = source_line;
  def->pc = pc;
}

static void insert_builtin(PAREN_SYMBOLS* p, const BUILTIN* b, BCODE* bcode) {
  insert_paren_symbol(p, bcode_name_entry(bcode, b->name), b->type, PK_BUILTIN);
}

static void insert_builtins(PAREN_SYMBOLS* p, BCODE* bcode) {
  const BUILTIN* b;

  for (unsigned i = 0; b = builtin_number(i); i++)
    insert_builtin(p, b, bcode);
}

struct gosub {
  unsigned pc;
  unsigned source_line;
};

#define MAX_NUM_STACK (16)
#define MAX_STR_STACK (8)
#define MAX_RETURN_STACK (8)
#define MAX_FOR (8)
#define TAB_SIZE (8)

typedef struct {
  BCODE* bc;
  double stack[MAX_NUM_STACK];
  char* strstack[MAX_STR_STACK];
  NUMVARS numvars;
  STRVARS strvars;
  PAREN_SYMBOLS paren;
  struct gosub retstack[MAX_RETURN_STACK];
  bool stopped;
  unsigned pc;
  unsigned sp;
  unsigned ssp;
  unsigned rsp;
  unsigned col;
  unsigned source_line;
  // FOR
  struct for_loop {
    unsigned line;
    int var;
    double step;
    double limit;
    unsigned pc;
  } for_stack[MAX_FOR];
  unsigned for_sp;
  // DATA
  unsigned data;
  // FN
  struct {
    unsigned pc;
    unsigned source_line;
    unsigned numvars_count;
  } fn;
  // INPUT
  char input[128];
  int inp;
  unsigned input_pc;
  // behaviour options
  bool trace_basic;
  bool trace_for;
  unsigned short array_base;
  bool strict_dim;
  bool strict_for;
  bool strict_on;
  bool strict_variables;
  bool input_prompt;
} VM;

static void init_vm(VM* vm, BCODE* bc, bool trace_basic, bool trace_for) {
  vm->bc = bc;
  vm->numvars.count = 0;
  vm->strvars.count = 0;
  init_paren_symbols(&vm->paren);
  vm->stopped = false;
  vm->pc = 0;
  vm->sp = 0;
  vm->ssp = 0;
  vm->rsp = 0;
  vm->col = 1;
  vm->source_line = 0;
  vm->trace_basic = trace_basic;
  vm->trace_for = trace_for;
  vm->array_base = 0;
  vm->for_sp = 0;
  vm->data = 0;
  vm->fn.pc = -1;
  vm->inp = -1;
  vm->input_pc = 0;
  vm->strict_dim = false;
  vm->strict_for = false;
  vm->strict_on = false;
  vm->strict_variables = false;
  vm->input_prompt = true;
  insert_builtins(&vm->paren, vm->bc);
}

static void deinit_vm(VM* vm) {
  while (vm->ssp > 0) {
    vm->ssp--;
    efree(vm->strstack[vm->ssp]);
  }
  for (unsigned i = 0; i < vm->strvars.count; i++)
    efree(vm->strvars.vars[i].val);
  deinit_paren_symbols(&vm->paren);
  vm->stopped = true;
  vm->pc = vm->bc->used;
  vm->sp = 0;
  vm->rsp = 0;
  vm->col = 1;
  vm->source_line = 0;
  vm->for_sp = 0;
  vm->data = 0;
  vm->fn.pc = -1;
  vm->input[0] = '\0';
  vm->inp = 0;
}

static void run_error(const VM* vm, const char* fmt, ...) {
  fputs("Runtime error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  print_source_line(vm->bc->source, vm->source_line, stderr);

  exit(EXIT_FAILURE);
}

static double pop(VM* vm) {
  if (vm->sp == 0)
    run_error(vm, "numeric stack empty\n");
  vm->sp--;
  return vm->stack[vm->sp];
}

static unsigned pop_unsigned(VM* vm) {
  double x = pop(vm);
  if (x < 0 || floor(x) != x)
    run_error(vm, "non-negative integer was expected: %g\n", x);
  if (x > (unsigned)(-1))
    run_error(vm, "out of range: %g\n", x);
  return (unsigned) x;
}

static int pop_logic(VM* vm) {
  double x = pop(vm);
  if (x < INT_MIN || x > INT_MAX || x != floor(x))
    run_error(vm, "invalid logical value: %g\n", x);
  return (int) x;
}

static void push(VM* vm, double num) {
  if (vm->sp >= MAX_NUM_STACK)
    fatal("numeric stack overflow\n");
  vm->stack[vm->sp++] = num;
}

static void push_logic(VM* vm, int n) {
  push(vm, n ? -1 : 0);
}

static char* pop_str(VM* vm) {
  if (vm->ssp == 0)
    run_error(vm, "string stack empty\n");
  vm->ssp--;
  return vm->strstack[vm->ssp];
}

static void push_str(VM* vm, const char* s) {
  if (vm->ssp >= MAX_STR_STACK)
    run_error(vm, "string stack overflow\n");
  vm->strstack[vm->ssp++] = estrdup(s ? s : "");
}

static const char* numvar_name(const VM* vm, unsigned var) {
  if (var >= vm->numvars.count)
    run_error(vm, "Internal error: numvar_name: variable number out of range\n");
  return bcode_name(vm->bc, vm->numvars.vars[var].name);
}

static void dump_numvars(const VM* vm, FILE* fp) {
  for (unsigned i = 0; i < vm->numvars.count; i++)
    fprintf(fp, "%s=%g\n", bcode_name(vm->bc, vm->numvars.vars[i].name), vm->numvars.vars[i].val);
}

static int lookup_numvar_name(VM* vm, unsigned name) {
  for (int i = vm->numvars.count - 1; i >= 0; i--) {
    if (vm->numvars.vars[i].name == name)
      return i;
  }

  return -1;
}

static void check_name(VM* vm, unsigned name) {
  if (name >= stringlist_count(&vm->bc->names))
    run_error(vm, "internal error: name index out of range\n");
}

static int insert_numvar_name(VM* vm, unsigned name) {
  check_name(vm, name);

  if (vm->numvars.count >= MAX_NUM_VAR) {
    dump_numvars(vm, stderr);
    run_error(vm, "too many numeric variables\n");
  }

  int i = vm->numvars.count++;
  vm->numvars.vars[i].name = name;
  vm->numvars.vars[i].val = 0;
  return i;
}

static void set_numvar_name(VM* vm, unsigned name, double val) {
  int j = lookup_numvar_name(vm, name);
  if (j < 0)
    j = insert_numvar_name(vm, name);
  vm->numvars.vars[j].val = val;
}

static int lookup_strvar_name(VM* vm, unsigned name) {
  for (int i = vm->strvars.count - 1; i >= 0; i--) {
    if (vm->strvars.vars[i].name == name)
      return i;
  }

  return -1;
}

static int insert_strvar_name(VM* vm, unsigned name) {
  check_name(vm, name);

  if (vm->strvars.count >= MAX_STR_VAR)
    fatal("too many string variables\n");

  int i = vm->strvars.count++;
  vm->strvars.vars[i].name = name;
  vm->strvars.vars[i].val = NULL;
  return i;
}

static void set_strvar_name(VM* vm, unsigned name, char* s) {
  int j = lookup_strvar_name(vm, name);
  if (j < 0)
    j = insert_strvar_name(vm, name);
  efree(vm->strvars.vars[j].val);
  vm->strvars.vars[j].val = s;
}

static double* numeric_element(VM* vm, struct numeric_array * p, unsigned name, unsigned dimensions) {
  unsigned indexes[MAX_DIMENSIONS];
  for (unsigned i = 0; i < dimensions; i++)
    indexes[i] = pop_unsigned(vm);
  double* addr = NULL;
  if (!compute_numeric_element(p, dimensions, indexes, &addr))
    run_error(vm, "array indexes invalid or out of range: %s\n", bcode_name(vm->bc, name));
  return addr;
}

static void dimension_numeric(VM* vm, unsigned name, unsigned short dimensions) {
  unsigned max[MAX_DIMENSIONS];

  for (unsigned i = 0; i < dimensions; i++)
    max[i] = pop_unsigned(vm);

  if (insert_numeric_array(&vm->paren, name, vm->array_base, dimensions, max) == NULL)
    run_error(vm, "invalid dimensions: %s\n", bcode_name(vm->bc, name));
}

static PAREN_SYMBOL* dimension_numeric_auto(VM* vm, unsigned name, unsigned short dimensions) {
  unsigned max[MAX_DIMENSIONS];

  for (unsigned i = 0; i < dimensions; i++) {
    max[i] = pop_unsigned(vm);
    if (max[i] < 10)
      max[i] = 10;
  }

  PAREN_SYMBOL* sym = insert_numeric_array(&vm->paren, name, vm->array_base, dimensions, max);
  if (sym == NULL)
    run_error(vm, "invalid dimensions: %s\n", bcode_name(vm->bc, name));
  return sym;
}

static void redimension_numeric(VM* vm, PAREN_SYMBOL* sym, unsigned dimensions) {
  assert(sym->kind == PK_ARRAY);
  assert(sym->type == TYPE_NUM);

  unsigned max[MAX_DIMENSIONS];

  for (unsigned i = 0; i < dimensions; i++)
    max[i] = pop_unsigned(vm);

  if (!replace_numeric_array(sym, vm->array_base, dimensions, max))
    run_error(vm, "invalid dimensions: %s\n", bcode_name(vm->bc, sym->name));
}

static void set_numeric_element_name(VM* vm, unsigned name, unsigned dimensions, double val) {
  PAREN_SYMBOL* sym = lookup_paren_name(&vm->paren, name);
  if (sym == NULL)
    sym = dimension_numeric_auto(vm, name, dimensions);
  else if (sym->kind != PK_ARRAY || sym->type != TYPE_NUM)
    run_error(vm, "not a numeric array: %s\n", bcode_name(vm->bc, name));
  *numeric_element(vm, sym->u.numarr, name, dimensions) = val;
}

static void set_numeric_name(VM* vm, unsigned name, unsigned dimensions, double val) {
  if (dimensions == 0)
    set_numvar_name(vm, name, val);
  else
    set_numeric_element_name(vm, name, dimensions, val);
}

static void dimension_string(VM* vm, unsigned name, unsigned short dimensions) {
  unsigned max[MAX_DIMENSIONS];

  for (unsigned i = 0; i < dimensions; i++) {
    max[i] = pop_unsigned(vm);
    if (max[i] < 10)
      max[i] = 10;
  }

  if (insert_string_array(&vm->paren, name, vm->array_base, dimensions, max) == NULL)
    run_error(vm, "invalid dimensions: %s\n", bcode_name(vm->bc, name));
}

static PAREN_SYMBOL* dimension_string_auto(VM* vm, unsigned name, unsigned short dimensions) {
  unsigned max[MAX_DIMENSIONS];

  for (unsigned i = 0; i < dimensions; i++) {
    max[i] = pop_unsigned(vm);
    if (max[i] < 10)
      max[i] = 10;
  }

  PAREN_SYMBOL* sym = insert_string_array(&vm->paren, name, vm->array_base, dimensions, max);
  if (sym == NULL)
    run_error(vm, "invalid dimensions: %s\n", bcode_name(vm->bc, name));
  return sym;
}

static void redimension_string(VM* vm, PAREN_SYMBOL* sym, unsigned dimensions) {
  assert(sym->kind == PK_ARRAY);
  assert(sym->type == TYPE_STR);

  unsigned max[MAX_DIMENSIONS];

  for (unsigned i = 0; i < dimensions; i++)
    max[i] = pop_unsigned(vm);

  if (!replace_string_array(sym, vm->array_base, dimensions, max))
    run_error(vm, "invalid dimensions: %s\n", bcode_name(vm->bc, sym->name));
}

static char* * string_element(VM* vm, struct string_array * p, unsigned name, unsigned dimensions) {
  unsigned indexes[MAX_DIMENSIONS];
  for (unsigned i = 0; i < dimensions; i++)
    indexes[i] = pop_unsigned(vm);
  char* * addr = NULL;
  if (!compute_string_element(p, dimensions, indexes, &addr))
    run_error(vm, "array indexes invalid or out of range: %s\n", bcode_name(vm->bc, name));
  assert(addr != NULL);
  return addr;
}

static void set_string_element(VM* vm, struct string_array * p, unsigned name, unsigned dimensions, char* val) {
  char* *addr = string_element(vm, p, name, dimensions);
  efree(*addr);
  *addr = val;
}

static void set_string_element_name(VM* vm, unsigned name, unsigned dimensions, char* val) {
  PAREN_SYMBOL* sym = lookup_paren_name(&vm->paren, name);
  if (sym == NULL)
    sym = dimension_string_auto(vm, name, dimensions);
  if (sym->kind != PK_ARRAY || sym->type != TYPE_STR)
    run_error(vm, "not a string array: %s\n", bcode_name(vm->bc, name));
  set_string_element(vm, sym->u.strarr, name, dimensions, val);
}

static void set_string_name(VM* vm, unsigned name, unsigned dimensions, char* val) {
  if (dimensions == 0)
    set_strvar_name(vm, name, val);
  else
    set_string_element_name(vm, name, dimensions, val);
}

static void execute(VM*);

static const char STRING_TOO_LONG[] = "string too long\n";

void run(BCODE* bc, bool trace_basic, bool trace_for, bool randomize) {
  assert(bc != NULL);
  VM vm;
  init_vm(&vm, bc, trace_basic, trace_for);

  if (randomize)
    srand((unsigned)time(NULL));

  while (vm.pc < bc->used && !vm.stopped)
    execute(&vm);

  if (vm.stopped) {
    print_source_line(vm.bc->source, vm.source_line, stdout);
    puts("Stopped");
  }
  else {
    if (vm.strict_for && vm.for_sp != 0) {
      struct for_loop * f = &vm.for_stack[vm.for_sp-1];
      const char* name = numvar_name(&vm, f->var);
      fprintf(stderr, "FOR without NEXT: %s\n", name);
      print_source_line(vm.bc->source, f->line, stderr);
    }
  }

  deinit_vm(&vm);
}

static unsigned find_basic_line(VM* vm, unsigned basic_line) {
  unsigned source_line;
  if (!bcode_find_basic_line(vm->bc, basic_line, &source_line))
    run_error(vm, "Line not found: %u\n", basic_line);
  return source_line;
}

static void check_paren_kind(VM* vm, const PAREN_SYMBOL* sym, int kind) {
  assert(sym != NULL);

  if (sym->kind != kind)
    run_error(vm, "name is %s, not %s: %s\n", paren_kind(sym->kind), paren_kind(kind), bcode_name(vm->bc, sym->name));
}

static void call_def(VM* vm, const struct def * def, unsigned name, unsigned params);
static void end_def(VM*);

static int compare_strings(VM*);

static int find_for(VM*, unsigned name);
static void dump_for(VM*, const char* tag);
static void dump_for_stack(VM*, const char* tag);

static const char* find_data(VM*);

static char* convert(const char* string, double *val);

static void execute(VM* vm) {
  assert(vm->pc < vm->bc->used);

  char buf[256];
  size_t sz;
  unsigned long u, v;
  int j;
  char* s;
  const char* S;
  int c;
  char* t;
  const BINST* i = vm->bc->inst + vm->pc;
  double x;
  unsigned k;
  int logic1, logic2;
  struct for_loop * f;
  int var;
  int si;
  PAREN_SYMBOL* sym;

  switch (i->op) {
    case B_NOP:
      break;
    // source
    case B_LINE:
      vm->source_line = i->u.line;
      if (vm->trace_basic) {
        printf("[%u]", source_linenum(vm->bc->source, i->u.line));
        fflush(stdout);
      }
      break;
    // number
    case B_PUSH_NUM:
      push(vm, i->u.num);
      break;
    case B_GET_SIMPLE_NUM:
      j = lookup_numvar_name(vm, i->u.name);
      if (j < 0) {
        if (vm->strict_variables)
          run_error(vm, "Variable not found: %s\n", stringlist_item(&vm->bc->names, i->u.name));
        push(vm, 0);
      }
      else
        push(vm, vm->numvars.vars[j].val);
      break;
    case B_SET_SIMPLE_NUM:
      set_numvar_name(vm, i->u.name, pop(vm));
      break;
    case B_DIM_NUM:
      sym = lookup_paren_name(&vm->paren, i->u.param.name);
      if (sym) {
        check_paren_kind(vm, sym, PK_ARRAY);
        if (sym->type != TYPE_NUM)
          run_error(vm, "cannot redimension: not a numeric array: %s\n", bcode_name(vm->bc, i->u.param.name));
        redimension_numeric(vm, sym, i->u.param.params);
      }
      else
        dimension_numeric(vm, i->u.param.name, i->u.param.params);
      break;
    case B_GET_PAREN_NUM:
      sym = lookup_paren_name(&vm->paren, i->u.param.name);
      if (sym == NULL) {
        sym = dimension_numeric_auto(vm, i->u.param.name, i->u.param.params);
        push(vm, 0);
      }
      else {
        if (sym->type != TYPE_NUM)
          run_error(vm, "numeric array or function was expected: %s\n", bcode_name(vm->bc, i->u.param.name));
        switch (sym->kind) {
          case PK_ARRAY:
            push(vm, *numeric_element(vm, sym->u.numarr, i->u.param.name, i->u.param.params));
            break;
          case PK_DEF:
            call_def(vm, sym->u.def, i->u.param.name, i->u.param.params);
            break;
          default:
            fatal("internal error: unexpected kind of paren symbol\n");
        }
      }
      break;
    case B_SET_ARRAY_NUM:
      set_numeric_element_name(vm, i->u.param.name, i->u.param.params, pop(vm));
      break;
    case B_NEG:
      push(vm, - pop(vm));
      break;
    case B_ADD:
      x = pop(vm);
      push(vm, pop(vm) + x);
      break;
    case B_SUB:
      x = pop(vm);
      push(vm, pop(vm) - x);
      break;
    case B_MUL:
      x = pop(vm);
      push(vm, pop(vm) * x);
      break;
    case B_DIV:
      x = pop(vm);
      push(vm, pop(vm) / x);
      break;
    case B_POW:
      x = pop(vm);
      push(vm, pow(pop(vm), x));
      break;
    case B_EQ_NUM:
      x = pop(vm);
      push_logic(vm, pop(vm) == x);
      break;
    case B_LT_NUM:
      x = pop(vm);
      push_logic(vm, pop(vm) < x);
      break;
    case B_GT_NUM:
      x = pop(vm);
      push_logic(vm, pop(vm) > x);
      break;
    case B_NE_NUM:
      x = pop(vm);
      push_logic(vm, pop(vm) != x);
      break;
    case B_LE_NUM:
      x = pop(vm);
      push_logic(vm, pop(vm) <= x);
      break;
    case B_GE_NUM:
      x = pop(vm);
      push_logic(vm, pop(vm) >= x);
      break;
    case B_OR:
      logic2 = pop_logic(vm);
      logic1 = pop_logic(vm);
      push(vm, logic1 | logic2);
      break;
    case B_AND:
      logic2 = pop_logic(vm);
      logic1 = pop_logic(vm);
      push(vm, logic1 & logic2);
      break;
    case B_NOT:
      logic1 = pop_logic(vm);
      push(vm, ~logic1);
      break;
    // string
    case B_PUSH_STR:
      push_str(vm, i->u.str);
      break;
    case B_SET_SIMPLE_STR:
      set_strvar_name(vm, i->u.name, pop_str(vm));
      break;
    case B_GET_SIMPLE_STR:
      j = lookup_strvar_name(vm, i->u.name);
      if (j < 0) {
        if (vm->strict_variables)
          run_error(vm, "Variable not found: %s\n", stringlist_item(&vm->bc->names, i->u.name));
        push_str(vm, "");
      }
      else
        push_str(vm, vm->strvars.vars[j].val);
      break;
    case B_DIM_STR:
      sym = lookup_paren_name(&vm->paren, i->u.param.name);
      if (sym) {
        check_paren_kind(vm, sym, PK_ARRAY);
        if (sym->type != TYPE_STR)
          run_error(vm, "cannot redimension: not a string array: %s\n", bcode_name(vm->bc, i->u.param.name));
        redimension_string(vm, sym, i->u.param.params);
      }
      else
        dimension_string(vm, i->u.param.name, i->u.param.params);
      break;
    case B_GET_PAREN_STR:
      sym = lookup_paren_name(&vm->paren, i->u.param.name);
      if (sym == NULL) {
        sym = dimension_string_auto(vm, i->u.param.name, i->u.param.params);
        push_str(vm, "");
      }
      else {
        if (sym->type != TYPE_STR)
          run_error(vm, "string array or function was expected: %s\n", bcode_name(vm->bc, i->u.param.name));
        switch (sym->kind) {
          case PK_ARRAY:
            push_str(vm, *string_element(vm, sym->u.strarr, i->u.param.name, i->u.param.params));
            break;
          case PK_DEF:
            call_def(vm, sym->u.def, i->u.param.name, i->u.param.params);
            break;
          default:
            fatal("internal error: unexpected kind of paren symbol\n");
        }
      }
      break;
    case B_SET_ARRAY_STR:
      set_string_element_name(vm, i->u.param.name, i->u.param.params, pop_str(vm));
      break;
    case B_EQ_STR:
      push_logic(vm, compare_strings(vm) == 0);
      break;
    case B_NE_STR:
      push_logic(vm, compare_strings(vm) != 0);
      break;
    case B_LT_STR:
      push_logic(vm, compare_strings(vm) < 0);
      break;
    case B_GT_STR:
      push_logic(vm, compare_strings(vm) > 0);
      break;
    case B_LE_STR:
      push_logic(vm, compare_strings(vm) <= 0);
      break;
    case B_GE_STR:
      push_logic(vm, compare_strings(vm) >= 0);
      break;
    case B_CONCAT:
      t = pop_str(vm);
      s = pop_str(vm);
      sz = strlen(s) + strlen(t);
      if (sz + 1 > sizeof buf)
        run_error(vm, "concatenated string would be too long: %lu characters\n", (unsigned long)sz);
      strcpy(buf, s);
      strcat(buf, t);
      push_str(vm, buf);
      efree(s);
      efree(t);
      break;
    // control flow
    case B_END:
      vm->pc = vm->bc->used;
      return;
    case B_STOP:
      vm->stopped = true;
      return;
    case B_GOTO:
      vm->pc = find_basic_line(vm, i->u.line);
      return;
    case B_GOTRUE:
      if (pop(vm)) {
        vm->pc = find_basic_line(vm, i->u.line);
        return;
      }
      break;
    case B_GOSUB:
      if (vm->rsp >= MAX_RETURN_STACK)
        run_error(vm, "GOSUB is nested too deeply\n");
      vm->retstack[vm->rsp].pc = vm->pc;
      vm->retstack[vm->rsp].source_line = vm->source_line;
      vm->rsp++;
      vm->pc = find_basic_line(vm, i->u.line);
      return;
    case B_RETURN:
      if (vm->rsp == 0)
        run_error(vm, "RETURN without GOSUB\n");
      vm->rsp--;
      vm->pc = vm->retstack[vm->rsp].pc;
      vm->source_line = vm->retstack[vm->rsp].source_line;
      break;
    case B_FOR:
      if (vm->trace_for)
        dump_for(vm, "FOR");
      si = find_for(vm, i->u.name);
      if (si >= 0) {
        if (vm->strict_for)
          run_error(vm, "already inside FOR loop controlled by this variable: %s\n", numvar_name(vm, vm->for_stack[si].var));
        if (si != vm->for_sp - 1) {
          // bring inner loop to top of stack, for NEXT with no variable
          struct for_loop inner = vm->for_stack[si];
          for (k = si; k < vm->for_sp - 1; k++)
            vm->for_stack[k] = vm->for_stack[k + 1];
          vm->for_stack[vm->for_sp - 1] = inner;
          si = vm->for_sp - 1;
        }
      }
      else {
        if (vm->for_sp >= MAX_FOR) {
          dump_for_stack(vm, "overflow");
          run_error(vm, "FOR is nested too deeply\n");
        }
        si = vm->for_sp++;
      }
      f = &vm->for_stack[si];
      f->var = lookup_numvar_name(vm, i->u.name);
      if (f->var < 0)
        f->var = insert_numvar_name(vm, i->u.name);
      f->step = pop(vm);
      f->limit = pop(vm);
      vm->numvars.vars[f->var].val = pop(vm);
      f->line = vm->source_line;
      f->pc = vm->pc;
      if (vm->trace_for)
        dump_for_stack(vm, "final stack");
      break;
    case B_NEXT_VAR:
      if (vm->trace_for)
        dump_for(vm, "NEXT-VARIABLE");
      if (vm->for_sp == 0)
        run_error(vm, "NEXT without FOR\n");
      si = vm->for_sp - 1;
      f = &vm->for_stack[si];
      var = lookup_numvar_name(vm, i->u.name);
      if (var != f->var) {
        if (vm->strict_for) {
          const char* for_name = numvar_name(vm, f->var);
          const char* next_name = bcode_name(vm->bc, i->u.name);
          run_error(vm, "mismatched FOR variable: expecting %s, found %s\n", for_name, next_name);
        }
        si = find_for(vm, i->u.name);
        if (si < 0)
          run_error(vm, "NEXT without FOR: %s\n", bcode_name(vm->bc, i->u.name));
        f = &vm->for_stack[si];
      }
      x = vm->numvars.vars[f->var].val + f->step;
      if (f->step > 0 && x > f->limit || f->step < 0 && x < f->limit) {
        // remove FOR stack index si
        assert(vm->for_sp > 0);
        vm->for_sp--;
        for (k = si; k < vm->for_sp; k++)
          vm->for_stack[k] = vm->for_stack[k + 1];
      }
      else {
        vm->numvars.vars[f->var].val = x;
        vm->pc = f->pc;
      }
      if (vm->trace_for)
        dump_for_stack(vm, "final stack");
      break;
    case B_NEXT_IMP:
      if (vm->trace_for)
        dump_for(vm, "NEXT-IMPLICIT");
      if (vm->for_sp == 0)
        run_error(vm, "NEXT without FOR\n");
      si = vm->for_sp - 1;
      f = &vm->for_stack[si];
      x = vm->numvars.vars[f->var].val + f->step;
      if (f->step > 0 && x > f->limit || f->step < 0 && x < f->limit)
        vm->for_sp--;
      else {
        vm->numvars.vars[f->var].val = x;
        vm->pc = f->pc;
      }
      if (vm->trace_for)
        dump_for_stack(vm, "final stack");
      break;
    case B_DEF:
      sym = lookup_paren_name(&vm->paren, i->u.param.name);
      if (sym) {
        check_paren_kind(vm, sym, PK_DEF);
        replace_def(sym, i->u.param.params, vm->source_line, vm->pc);
      }
      else {
        int type = string_name(bcode_name(vm->bc, i->u.param.name)) ? TYPE_STR : TYPE_NUM;
        insert_def(&vm->paren, i->u.param.name, type, i->u.param.params, vm->source_line, vm->pc);
      }
      do {
        vm->pc++;
      } while (vm->pc < vm->bc->used && vm->bc->inst[vm->pc].op != B_END_DEF);
      break;
    case B_PARAM:
      run_error(vm, "internal error: run into parameter\n");
      break;
    case B_END_DEF:
      end_def(vm);
      break;
    case B_ON_GOTO:
      x = pop(vm);
      if (x != floor(x))
        run_error(vm, "ON value is invalid: %g\n", x);
      if (x < 1 || x > i->u.count) {
        if (vm->strict_on)
          run_error(vm, "ON value is out of range: %g\n", x);
        vm->pc += i->u.count + 1;
        return;
      }
      k = vm->pc + (unsigned) x;
      if (k >= vm->bc->used || vm->bc->inst[k].op != B_ON_LINE)
        run_error(vm, "internal error: ON-LINE expected\n");
      vm->pc = find_basic_line(vm, vm->bc->inst[k].u.line);
      return;
    case B_IF_THEN: // IF ... THEN statements  -- skip to next line if condition false
      if (!pop(vm)) {
        do {
          vm->pc++;
        } while (vm->pc < vm->bc->used && vm->bc->inst[vm->pc].op != B_LINE);
        return;
      }
      break;
    // output
    case B_PRINT_LN:
      putchar('\n');
      vm->col = 1;
      break;
    case B_PRINT_SPC:
      k = pop_unsigned(vm);
      while (k--)
        putchar(' '), vm->col++;
      break;
    case B_PRINT_TAB:
      k = pop_unsigned(vm);
      if (k < vm->col) {
        putchar('\n');
        vm->col = 1;
      }
      while (vm->col < k)
        putchar(' '), vm->col++;
      break;
    case B_PRINT_COMMA:
      do {
        putchar(' '), vm->col++;
      } while (vm->col % TAB_SIZE != 1);
      break;
    case B_PRINT_NUM:
      vm->col += printf(" %g ", pop(vm));
      break;
    case B_PRINT_STR:
      s = pop_str(vm);
      for (S = s; *S; S++) {
        putchar(*S);
        if (*S == '\n')
          vm->col = 1;
        else
          vm->col++;
      }
      efree(s);
      break;
    // input
    case B_INPUT_BUF:
      if (i->u.str)
        fputs(i->u.str, stdout);
      if (vm->input_prompt)
        fputs("? ", stdout);
      fflush(stdout);
      if (fgets(vm->input, sizeof vm->input, stdin) == NULL) {
        if (ferror(stdin))
          run_error(vm, "error reading input\n");
      }
      vm->inp = 0;
      vm->input_pc = vm->pc;
      break;
    case B_INPUT_END:
      while ((c = vm->input[vm->inp]) == ' ' || c == '\t' || c == '\n' || c == '\r')
        vm->inp++;
      if (c != '\0')
        puts("* Extra input was discarded *");
      break;
    case B_INPUT_SEP:
      while ((c = vm->input[vm->inp]) == ' ' || c == '\t')
        vm->inp++;
      if (c == ',') {
        vm->inp++;
        break;
      }
      puts("* More input items are expected *");
      vm->pc = vm->input_pc;
      return;
    case B_INPUT_NUM:
      t = convert(vm->input + vm->inp, &x);
      if (t != NULL && (*t == '\0' || *t == '\n' || *t == ',')) {
        set_numeric_name(vm, i->u.param.name, i->u.param.params, x);
        vm->inp = (int) (t - vm->input);
        break;
      }
      puts("* Invalid input *");
      vm->pc = vm->input_pc;
      return;
    case B_INPUT_STR:
      s = vm->input + vm->inp;
      while ((c = vm->input[vm->inp]) != '\0' && c != '\n' && c != ',')
        vm->inp++;
      vm->input[vm->inp] = '\0';
      s = estrdup(s);
      set_string_name(vm, i->u.param.name, i->u.param.params, s);
      vm->input[vm->inp] = c;
      break;
    case B_INPUT_LINE:
      s = strchr(vm->input, '\n');
      if (s)
        *s = '\0';
      s = estrdup(vm->input);
      set_string_name(vm, i->u.param.name, i->u.param.params, s);
      break;
    // inline data
    case B_DATA:
      break;
    case B_READ_NUM:
      S = find_data(vm);
      t = convert(S, &x);
      if (t == NULL || *t != '\0')
        run_error(vm, "numeric data expected: %s\n", S);
      set_numeric_name(vm, i->u.param.name, i->u.param.params, x);
      break;
    case B_READ_STR:
      S = find_data(vm);
      set_string_name(vm, i->u.param.name, i->u.param.params, estrdup(S));
      break;
    case B_RESTORE:
      vm->data = 0;
      break;
    // builtins
    case B_ASC:
      s = pop_str(vm);
      push(vm, s[0]);
      efree(s);
      break;
    case B_ABS:
      push(vm, fabs(pop(vm)));
      break;
    case B_ATN:
      push(vm, atan(pop(vm)));
      break;
    case B_CHR:
      x = pop(vm);
      if (x < 0 || x > 255 || x != floor(x))
        run_error(vm, "invalid character code: %g\n", x);
      buf[0] = (char) x;
      buf[1] = '\0';
      push_str(vm, buf);
      break;
    case B_COS:
      push(vm, cos(pop(vm)));
      break;
    case B_EXP:
      push(vm, exp(pop(vm)));
      break;
    case B_INT:
      push(vm, floor(pop(vm)));
      break;
    case B_LEFT:
      s = pop_str(vm);
      u = pop_unsigned(vm);
      sz = strlen(s);
      if (u > sz)
        u = (unsigned long)sz;
      if (u + 1 > sizeof buf)
        run_error(vm, STRING_TOO_LONG);
      strncpy(buf, s, u);
      buf[u] = '\0';
      push_str(vm, buf);
      efree(s);
      break;
    case B_LEN:
      s = pop_str(vm);
      push(vm, (double) strlen(s));
      efree(s);
      break;
    case B_LOG:
      x = pop(vm);
      if (x <= 0)
        run_error(vm, "invalid logarithm\n");
      push(vm, log(x));
      break;
    case B_MID3:
      s = pop_str(vm);
      v = pop_unsigned(vm);
      u = pop_unsigned(vm);
      sz = strlen(s);
      if (u < 1 || u > sz)
        run_error(vm, "string index out of range\n");
      if (v > sz - u + 1)
        v = (unsigned long)(sz - u + 1);
      if (v + 1 > sizeof buf)
        run_error(vm, STRING_TOO_LONG);
      strncpy(buf, s + u - 1, v);
      buf[v] = '\0';
      push_str(vm, buf);
      efree(s);
      break;
    case B_STR:
      x = pop(vm);
      sprintf(buf, "%g", x);
      push_str(vm, buf);
      break;
    case B_RIGHT:
      s = pop_str(vm);
      u = pop_unsigned(vm);
      sz = strlen(s);
      if (u > sz)
        u = (unsigned long)sz;
      if (u + 1 > sizeof buf)
        run_error(vm, STRING_TOO_LONG);
      strncpy(buf, s + sz - u, u);
      buf[u] = '\0';
      push_str(vm, buf);
      efree(s);
      break;
    case B_RND:
      pop(vm); // dummy argument
      do {
        x = (double) rand() / RAND_MAX;
      } while (x >= 1);
      push(vm, x);
      break;
    case B_SGN:
      x = pop(vm);
      if (x < 0)
        x = -1;
      else if (x > 0)
        x = 1;
      push(vm, x);
      break;
    case B_SIN:
      push(vm, sin(pop(vm)));
      break;
    case B_SQR:
      push(vm, sqrt(pop(vm)));
      break;
    case B_TAN:
      push(vm, tan(pop(vm)));
      break;
    case B_VAL:
      s = pop_str(vm);
      t = convert(s, &x);
      if (t == NULL || *t != '\0')
        run_error(vm, "invalid number: %s\n", s);
      efree(s);
      push(vm, x);
      break;
    // unknown opcode
    default:
      fputs("UNKNOWN OPCODE:\n", stderr);
      print_binst(vm->bc, vm->pc, stderr);
      run_error(vm, "unknown opcode: %u\n", i->op);
  }
  vm->pc++;
}

static char* convert(const char* s, double *val) {
  while (*s == ' ' || *s == '\t')
    s++;

  if (isdigit(*s) || *s == '.' || *s == '-') {
    char* end = NULL;
    *val = strtod(s, &end);
    while (*end == ' ' || *end == '\t')
      end++;
    return end;
  }

  return NULL;
}

static const char* find_data(VM* vm) {
  while (vm->data < vm->bc->used && vm->bc->inst[vm->data].op != B_DATA)
    vm->data++;

  if (vm->data >= vm->bc->used)
    run_error(vm, "out of data\n");

  const char* s = vm->bc->inst[vm->data++].u.str;
  if (s == NULL)
    run_error(vm, "internal error: null data\n");

  return s;
}

static void call_def(VM* vm, const struct def * def, unsigned name, unsigned params) {
  if (vm->fn.pc != -1)
    run_error(vm, "nested user-defined function calls are not allowed\n");

  if (params != def->params)
    run_error(vm, "unexpected number of parameters: %s: expected %u, received %u\n", bcode_name(vm->bc, name), def->params, params);

  if (vm->pc + params >= vm->bc->used)
    run_error(vm, "program corrupt: missing parameters: %s\n", bcode_name(vm->bc, name));

  vm->fn.pc = vm->pc;
  vm->fn.source_line = vm->source_line;
  vm->fn.numvars_count = vm->numvars.count;

  vm->pc = def->pc;
  vm->source_line = def->source_line;

  // top of stack is final parameter
  for ( ; params > 0; params--) {
    BINST* p = vm->bc->inst + vm->pc + params;
    if (p->op != B_PARAM) {
      print_binst(vm->bc, vm->pc, stderr);
      run_error(vm, "program corrupt: parameter expected\n");
    }
    int var = insert_numvar_name(vm, p->u.name);
    vm->numvars.vars[var].val = pop(vm);
  }

  vm->pc += def->params;
}

static void end_def(VM* vm) {
  if (vm->fn.pc == -1)
    run_error(vm, "unexpected END DEF\n");
  vm->pc = vm->fn.pc;
  vm->source_line = vm->fn.source_line;
  vm->numvars.count = vm->fn.numvars_count;
  vm->fn.pc = -1;
}

static int compare_strings(VM* vm) {
  char* t = pop_str(vm);
  char* s = pop_str(vm);
  int r = strcmp(s, t);
  efree(s);
  efree(t);
  return r;
}

static int find_for(VM* vm, unsigned name) {
  for (int j = 0; j < (int)vm->for_sp; j++) {
    int var = vm->for_stack[j].var;
    if (vm->numvars.vars[var].name == name)
      return j;
  }
  return -1;
}

static void dump_for(VM* vm, const char* tag) {
  printf("[%s]\n", tag);
  printf("-- line: %u %s\n", source_linenum(vm->bc->source, vm->source_line), source_text(vm->bc->source, vm->source_line));
  dump_for_stack(vm, "initial stack");
}

static void dump_for_stack(VM* vm, const char* tag) {
  printf("-- %s: ", tag);
  if (vm->for_sp == 0)
    fputs("empty", stdout);
  else {
    for (unsigned j = vm->for_sp; j > 0; j--) {
      int var = vm->for_stack[j-1].var;
      printf("%s = %g, %g; ", numvar_name(vm, var), vm->numvars.vars[var].val, vm->for_stack[j-1].limit);
    }
  }
  putc('\n', stdout);
}