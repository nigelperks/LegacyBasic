// Legacy BASIC
// Copyright (c) 2022-3 Nigel Perks

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include "run.h"
#include "bcode.h"
#include "paren.h"
#include "utils.h"
#include "interrupt.h"
#include "parse.h"
#include "os.h"

#define MAX_NUM_STACK (16)
#define MAX_STR_STACK (8)
#define MAX_RETURN_STACK (8)
#define MAX_FOR (8)
#define TAB_SIZE (8)

struct vm {
  SOURCE* program_source;
  BCODE* program_bcode;
  ENV* env;
  const SOURCE* source;
  const BCODE* bc;
  double stack[MAX_NUM_STACK];
  char* strstack[MAX_STR_STACK];
  struct gosub {
    const SOURCE* source;
    unsigned source_line;
    const BCODE* bcode;
    unsigned pc;
  } retstack[MAX_RETURN_STACK];
  bool stopped;
  unsigned pc;
  unsigned sp;
  unsigned ssp;
  unsigned rsp;
  unsigned col;
  unsigned source_line;
  // FOR
  struct for_loop {
    const SOURCE* source;
    unsigned source_line;
    int var;
    double step;
    double limit;
    const BCODE* bc;
    unsigned pc;
  } for_stack[MAX_FOR];
  unsigned for_sp;
  // DATA
  unsigned data;
  // FN
  struct {
    const BCODE* bc;
    unsigned pc;
    const SOURCE* source;
    unsigned source_line;
    unsigned numvars_count;
  } fn;
  // INPUT
  char input[128];
  int inp;
  unsigned input_pc;
  // behaviour options
  bool keywords_anywhere;
  bool trace_basic;
  bool trace_for;
  unsigned short array_base;
  bool strict_dim;
  bool strict_for;
  bool strict_on;
  bool strict_variables;
  bool input_prompt;
  bool verbose;
  // run-time error-catching
  jmp_buf errjmp;
};

// determine whether control state refers to immediate-mode code
// rather than just program code
static bool immediate_state(VM* vm) {
  for (unsigned i = 0; i < vm->rsp; i++) {
    assert(vm->retstack[i].bcode != NULL);
    if (vm->retstack[i].bcode != vm->program_bcode)
      return true;
  }

  for (unsigned i = 0; i < vm->for_sp; i++) {
    assert(vm->for_stack[i].bc != NULL);
    if (vm->for_stack[i].bc != vm->program_bcode)
      return true;
  }

  if (vm->fn.bc != NULL && vm->fn.bc != vm->program_bcode)
    return true;

  return false;
}

// reset control state so that no GOSUB, FOR, function call is in progress
static void reset_vm(VM* vm) {
  while (vm->ssp > 0) {
    vm->ssp--;
    efree(vm->strstack[vm->ssp]);
  }
  vm->stopped = false;
  vm->pc = 0;
  vm->sp = 0;
  vm->ssp = 0;
  vm->rsp = 0;
  vm->col = 1;
  vm->source_line = 0;
  vm->for_sp = 0;
  vm->data = 0;
  vm->fn.bc = NULL;
  vm->inp = -1;
  vm->input_pc = 0;
}

VM* new_vm(bool keywords_anywhere, bool trace_basic, bool trace_for) {
  VM* vm = ecalloc(1, sizeof *vm);
  vm->env = new_environment_with_builtins();
  reset_vm(vm);
  vm->keywords_anywhere = keywords_anywhere;
  vm->trace_basic = trace_basic;
  vm->trace_for = trace_for;
  vm->input_prompt = true;
  return vm;
}

void delete_vm(VM* vm) {
  if (vm) {
    reset_vm(vm);
    efree(vm);
  }
}

// Flag source program as changed and compiled program as out of date.
// Do not clear environment.
static void program_changed(VM* vm) {
  if (vm->program_bcode) {
    delete_bcode(vm->program_bcode);
    vm->program_bcode = NULL;
  }
}

// If a source line exists with the given line number, delete it,
// and flag the program as changed since the last compilation.
void vm_delete_source_line(VM* vm, unsigned num) {
  assert(vm != NULL);
  if (vm->program_source) {
    unsigned i;
    if (find_source_linenum(vm->program_source, num, &i)) {
      delete_source_line(vm->program_source, i);
      program_changed(vm);
    }
  }
}

void vm_enter_source_line(VM* vm, unsigned num, const char* text) {
  if (vm->program_source == NULL)
    vm->program_source = new_source(NULL);

  enter_source_line(vm->program_source, num, text);
  program_changed(vm);
}

static void run_error(VM* vm, const char* fmt, ...) {
  fputs("Runtime error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  if (vm->source && vm->source_line) {
    print_source_line(vm->source, vm->source_line, stderr);
    putc('\n', stderr);
  }

  longjmp(vm->errjmp, 1);
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

static unsigned pop_index(VM* vm) {
  double x = pop(vm);
  if (x < 0 || floor(x) != x)
    run_error(vm, "non-negative array index was expected: %g\n", x);
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
    run_error(vm, "numeric stack overflow\n");
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

static const char* numvar_name(VM* vm, unsigned var) {
  if (var >= vm->env->numvars.count)
    run_error(vm, "Internal error: numvar_name: variable number out of range\n");
  return stringlist_item(vm->env->names, vm->env->numvars.vars[var].name);
}

static void dump_numvars(const VM* vm, FILE* fp) {
  for (unsigned i = 0; i < vm->env->numvars.count; i++)
    fprintf(fp, "%s=%g\n", stringlist_item(vm->env->names, vm->env->numvars.vars[i].name), vm->env->numvars.vars[i].val);
}

static int lookup_numvar_name(VM* vm, unsigned name) {
  for (int i = vm->env->numvars.count - 1; i >= 0; i--) {
    if (vm->env->numvars.vars[i].name == name)
      return i;
  }

  return -1;
}

static void check_name(VM* vm, unsigned name) {
  if (name >= stringlist_count(vm->env->names))
    run_error(vm, "internal error: name index out of range\n");
}

static int insert_numvar_name(VM* vm, unsigned name) {
  check_name(vm, name);

  if (vm->env->numvars.count >= MAX_NUM_VAR) {
    dump_numvars(vm, stderr);
    run_error(vm, "too many numeric variables\n");
  }

  int i = vm->env->numvars.count++;
  vm->env->numvars.vars[i].name = name;
  vm->env->numvars.vars[i].val = 0;
  return i;
}

static void set_numvar_name(VM* vm, unsigned name, double val) {
  int j = lookup_numvar_name(vm, name);
  if (j < 0)
    j = insert_numvar_name(vm, name);
  vm->env->numvars.vars[j].val = val;
}

static int lookup_strvar_name(VM* vm, unsigned name) {
  for (int i = vm->env->strvars.count - 1; i >= 0; i--) {
    if (vm->env->strvars.vars[i].name == name)
      return i;
  }

  return -1;
}

static int insert_strvar_name(VM* vm, unsigned name) {
  check_name(vm, name);

  if (vm->env->strvars.count >= MAX_STR_VAR)
    run_error(vm, "too many string variables\n");

  int i = vm->env->strvars.count++;
  vm->env->strvars.vars[i].name = name;
  vm->env->strvars.vars[i].val = NULL;
  return i;
}

static void set_strvar_name(VM* vm, unsigned name, char* s) {
  int j = lookup_strvar_name(vm, name);
  if (j < 0)
    j = insert_strvar_name(vm, name);
  efree(vm->env->strvars.vars[j].val);
  vm->env->strvars.vars[j].val = s;
}

static void pop_indexes(VM* vm, unsigned* indexes, unsigned dimensions) {
  assert(dimensions <= MAX_DIMENSIONS);
  for (unsigned i = 0; i < dimensions; i++)
    indexes[i] = pop_index(vm);
}

static double* numeric_element(VM* vm, struct numeric_array * p, unsigned name, unsigned dimensions, unsigned* indexes) {
  double* addr = NULL;
  if (!compute_numeric_element(p, dimensions, indexes, &addr))
    run_error(vm, "array indexes invalid or out of range: %s\n", stringlist_item(vm->env->names, name));
  return addr;
}

static void dimension_numeric(VM* vm, unsigned name, unsigned short dimensions) {
  unsigned max[MAX_DIMENSIONS];

  for (unsigned i = 0; i < dimensions; i++)
    max[i] = pop_index(vm);

  if (insert_numeric_array(&vm->env->paren, name, vm->array_base, dimensions, max) == NULL)
    run_error(vm, "invalid dimensions: %s\n", stringlist_item(vm->env->names, name));
}

static PAREN_SYMBOL* dimension_numeric_auto(VM* vm, unsigned name, unsigned short dimensions, unsigned* indexes) {
  unsigned max[MAX_DIMENSIONS];

  for (unsigned i = 0; i < dimensions; i++) {
    max[i] = indexes[i];
    if (max[i] < 10)
      max[i] = 10;
  }

  PAREN_SYMBOL* sym = insert_numeric_array(&vm->env->paren, name, vm->array_base, dimensions, max);
  if (sym == NULL)
    run_error(vm, "invalid dimensions: %s\n", stringlist_item(vm->env->names, name));
  return sym;
}

static void redimension_numeric(VM* vm, PAREN_SYMBOL* sym, unsigned dimensions) {
  assert(sym->kind == PK_ARRAY);
  assert(sym->type == TYPE_NUM);

  unsigned max[MAX_DIMENSIONS];

  for (unsigned i = 0; i < dimensions; i++)
    max[i] = pop_index(vm);

  if (!replace_numeric_array(sym, vm->array_base, dimensions, max))
    run_error(vm, "invalid dimensions: %s\n", stringlist_item(vm->env->names, sym->name));
}

static void set_numeric_element_name(VM* vm, unsigned name, unsigned dimensions, double val) {
  unsigned indexes[MAX_DIMENSIONS];
  pop_indexes(vm, indexes, dimensions);

  PAREN_SYMBOL* sym = lookup_paren_name(&vm->env->paren, name);
  if (sym == NULL)
    sym = dimension_numeric_auto(vm, name, dimensions, indexes);
  else if (sym->kind != PK_ARRAY || sym->type != TYPE_NUM)
    run_error(vm, "not a numeric array: %s\n", stringlist_item(vm->env->names, name));

  *numeric_element(vm, sym->u.numarr, name, dimensions, indexes) = val;
}

static void set_numeric_name(VM* vm, unsigned name, unsigned dimensions, double val) {
  if (dimensions == 0)
    set_numvar_name(vm, name, val);
  else
    set_numeric_element_name(vm, name, dimensions, val);
}

static void dimension_string(VM* vm, unsigned name, unsigned short dimensions) {
  unsigned indexes[MAX_DIMENSIONS];
  pop_indexes(vm, indexes, dimensions);
  if (insert_string_array(&vm->env->paren, name, vm->array_base, dimensions, indexes) == NULL)
    run_error(vm, "invalid dimensions: %s\n", stringlist_item(vm->env->names, name));
}

static PAREN_SYMBOL* dimension_string_auto(VM* vm, unsigned name, unsigned short dimensions, unsigned* indexes) {
  unsigned max[MAX_DIMENSIONS];

  for (unsigned i = 0; i < dimensions; i++) {
    max[i] = indexes[i];
    if (max[i] < 10)
      max[i] = 10;
  }

  PAREN_SYMBOL* sym = insert_string_array(&vm->env->paren, name, vm->array_base, dimensions, max);
  if (sym == NULL)
    run_error(vm, "invalid dimensions: %s\n", stringlist_item(vm->env->names, name));
  return sym;
}

static void redimension_string(VM* vm, PAREN_SYMBOL* sym, unsigned dimensions) {
  assert(sym->kind == PK_ARRAY);
  assert(sym->type == TYPE_STR);

  unsigned max[MAX_DIMENSIONS];

  for (unsigned i = 0; i < dimensions; i++)
    max[i] = pop_index(vm);

  if (!replace_string_array(sym, vm->array_base, dimensions, max))
    run_error(vm, "invalid dimensions: %s\n", stringlist_item(vm->env->names, sym->name));
}

static char* * string_element(VM* vm, struct string_array * p, unsigned name, unsigned dimensions, unsigned *indexes) {
  char* * addr = NULL;
  if (!compute_string_element(p, dimensions, indexes, &addr))
    run_error(vm, "array indexes invalid or out of range: %s\n", stringlist_item(vm->env->names, name));
  assert(addr != NULL);
  return addr;
}

static void set_string_element(VM* vm, struct string_array * p, unsigned name, unsigned dimensions, unsigned* indexes, char* val) {
  char* *addr = string_element(vm, p, name, dimensions, indexes);
  efree(*addr);
  *addr = val;
}

static void set_string_element_name(VM* vm, unsigned name, unsigned dimensions, char* val) {
  unsigned indexes[MAX_DIMENSIONS];
  pop_indexes(vm, indexes, dimensions);

  PAREN_SYMBOL* sym = lookup_paren_name(&vm->env->paren, name);
  if (sym == NULL)
    sym = dimension_string_auto(vm, name, dimensions, indexes);
  if (sym->kind != PK_ARRAY || sym->type != TYPE_STR)
    run_error(vm, "not a string array: %s\n", stringlist_item(vm->env->names, name));
  set_string_element(vm, sym->u.strarr, name, dimensions, indexes, val);
}

static void set_string_name(VM* vm, unsigned name, unsigned dimensions, char* val) {
  if (dimensions == 0)
    set_strvar_name(vm, name, val);
  else
    set_string_element_name(vm, name, dimensions, val);
}

static void execute(VM*);

static const char STRING_TOO_LONG[] = "string too long\n";

void vm_new_program(VM* vm) {
  assert(vm != NULL);
  if (vm->program_source)
    clear_source(vm->program_source);
  program_changed(vm);
}

void vm_take_source(VM* vm, SOURCE* source) {
  assert(vm != NULL);
  assert(source != NULL);
  if (vm->program_source)
    clear_source(vm->program_source);
  vm->program_source = source;
  program_changed(vm);
}

unsigned vm_source_lines(const VM* vm) {
  assert(vm != NULL);
  return vm->program_source ? source_lines(vm->program_source) : 0;
}

unsigned vm_source_linenum(const VM* vm, unsigned index) {
  assert(vm != NULL);
  assert(vm->program_source != NULL);
  return source_linenum(vm->program_source, index);
}

const char* vm_source_text(const VM* vm, unsigned index) {
  assert(vm != NULL);
  assert(vm->program_source != NULL);
  return source_text(vm->program_source, index);
}

void vm_new_environment(VM* vm) {
  assert(vm != NULL);
  delete_environment(vm->env);
  vm->env = new_environment_with_builtins();
}

void vm_clear_environment(VM* vm) {
  assert(vm != NULL);
  clear_environment(vm->env);
}

static bool ensure_program_compiled(VM*);
static void run(VM*);

void run_immediate(VM* vm, const SOURCE* source, bool keywords_anywhere) {
  assert(vm != NULL);
  assert(source != NULL);

  if (ensure_program_compiled(vm)) {
    BCODE* bcode = parse_source(source, vm->env->names, keywords_anywhere);
    if (bcode) {
      vm->source = source;
      vm->source_line = 0;
      vm->bc = bcode;
      run(vm);
      if (immediate_state(vm))
        reset_vm(vm);
      delete_bcode(bcode);
    }
  }
}

void run_program(VM* vm) {
  assert(vm != NULL);

  if (vm->program_source == NULL)
    return;

  if (ensure_program_compiled(vm)) {
    reset_vm(vm);
    vm->source = vm->program_source;
    vm->source_line = 0;
    vm->bc = vm->program_bcode;
    run(vm);
  }
}

static void run(VM* vm) {
  vm->pc = 0;
  vm->stopped = false;
  if (setjmp(vm->errjmp) == 0) {
    while (vm->pc < vm->bc->used && !vm->stopped && !interrupted)
      execute(vm);
  }

  if (interrupted)
    puts("Break");
  else if (vm->stopped) {
    print_source_line(vm->source, vm->source_line, stdout);
    putchar('\n');
    puts("Stopped");
  }
  else {
    if (vm->strict_for && vm->for_sp != 0) {
      struct for_loop * f = &vm->for_stack[vm->for_sp-1];
      const char* name = numvar_name(vm, f->var);
      fprintf(stderr, "FOR without NEXT: %s\n", name);
      print_source_line(f->source, f->source_line, stderr);
      putc('\n', stderr);
    }
  }
}

// Recompile the stored program if its source has changed since last compilation.
// Return false on compilation error, true otherwise.
static bool ensure_program_compiled(VM* vm) {
  assert(vm != NULL);

  if (vm->program_source != NULL && vm->program_bcode == NULL) {
    if (vm->verbose)
      puts("Compiling...");
    reset_vm(vm);
    vm->program_bcode = parse_source(vm->program_source, vm->env->names, vm->keywords_anywhere);
    return vm->program_bcode != NULL;
  }

  return true;
}

void vm_print_bcode(VM* vm) {
  assert(vm != NULL);

  if (vm->program_source == NULL)
    return;

  if (ensure_program_compiled(vm))
    print_bcode(vm->program_bcode, vm->program_source, vm->env->names, stdout);
}

static void go_to(VM* vm, unsigned basic_line) {
  unsigned bcode_line;
  if (!bcode_find_basic_line(vm->program_bcode, basic_line, vm->program_source, &bcode_line))
    run_error(vm, "Line not found: %u\n", basic_line);

  vm->pc = bcode_line;
  vm->bc = vm->program_bcode;
  vm->source = vm->program_source;
}

static void push_return(VM* vm, unsigned pc_continue) {
  if (vm->rsp >= MAX_RETURN_STACK)
    run_error(vm, "GOSUB is nested too deeply\n");
  vm->retstack[vm->rsp].source = vm->source;
  vm->retstack[vm->rsp].source_line = vm->source_line;
  vm->retstack[vm->rsp].bcode = vm->bc;
  vm->retstack[vm->rsp].pc = pc_continue;
  vm->rsp++;
}

static void pop_return(VM* vm) {
  if (vm->rsp == 0)
    run_error(vm, "RETURN without GOSUB\n");
  vm->rsp--;
  vm->source = vm->retstack[vm->rsp].source;
  vm->source_line = vm->retstack[vm->rsp].source_line;
  vm->bc = vm->retstack[vm->rsp].bcode;
  vm->pc = vm->retstack[vm->rsp].pc; // PC to continue from
}

static void check_paren_kind(VM* vm, const PAREN_SYMBOL* sym, int kind) {
  assert(sym != NULL);

  if (sym->kind != kind)
    run_error(vm, "name is %s, not %s: %s\n", paren_kind(sym->kind), paren_kind(kind), stringlist_item(vm->env->names, sym->name));
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

  const BINST* const i = vm->bc->inst + vm->pc;

  switch (i->op) {
    case B_NOP:
      break;
    // source
    case B_LINE:
      vm->source_line = i->u.line;
      if (vm->trace_basic) {
        printf("[%u]", source_linenum(vm->source, i->u.line));
        fflush(stdout);
      }
      break;
    // whole environment
    case B_CLEAR:
      vm_clear_environment(vm);
      break;
    // number
    case B_PUSH_NUM:
      push(vm, i->u.num);
      break;
    case B_GET_SIMPLE_NUM: {
      int j = lookup_numvar_name(vm, i->u.name);
      if (j < 0) {
        if (vm->strict_variables)
          run_error(vm, "Variable not found: %s\n", stringlist_item(vm->env->names, i->u.name));
        push(vm, 0);
      }
      else
        push(vm, vm->env->numvars.vars[j].val);
      break;
    }
    case B_SET_SIMPLE_NUM:
      set_numvar_name(vm, i->u.name, pop(vm));
      break;
    case B_DIM_NUM: {
      PAREN_SYMBOL* sym = lookup_paren_name(&vm->env->paren, i->u.param.name);
      if (sym) {
        check_paren_kind(vm, sym, PK_ARRAY);
        if (sym->type != TYPE_NUM)
          run_error(vm, "cannot redimension: not a numeric array: %s\n", stringlist_item(vm->env->names, i->u.param.name));
        redimension_numeric(vm, sym, i->u.param.params);
      }
      else
        dimension_numeric(vm, i->u.param.name, i->u.param.params);
      break;
    }
    case B_GET_PAREN_NUM: {
      PAREN_SYMBOL* sym = lookup_paren_name(&vm->env->paren, i->u.param.name);
      if (sym == NULL) {
        unsigned indexes[MAX_DIMENSIONS];
        pop_indexes(vm, indexes, i->u.param.params);
        sym = dimension_numeric_auto(vm, i->u.param.name, i->u.param.params, indexes);
        push(vm, 0);
        break;
      }
      if (sym->type != TYPE_NUM)
        run_error(vm, "numeric array or function was expected: %s\n", stringlist_item(vm->env->names, i->u.param.name));
      switch (sym->kind) {
        case PK_ARRAY: {
          unsigned indexes[MAX_DIMENSIONS];
          pop_indexes(vm, indexes, i->u.param.params);
          push(vm, *numeric_element(vm, sym->u.numarr, i->u.param.name, i->u.param.params, indexes));
          break;
        }
        case PK_DEF:
          call_def(vm, sym->u.def, i->u.param.name, i->u.param.params);
          break;
        default:
          fatal("internal error: unexpected kind of paren symbol\n");
      }
      break;
    }
    case B_SET_ARRAY_NUM:
      set_numeric_element_name(vm, i->u.param.name, i->u.param.params, pop(vm));
      break;
    case B_NEG:
      push(vm, - pop(vm));
      break;
    case B_ADD: {
      double x = pop(vm);
      push(vm, pop(vm) + x);
      break;
    }
    case B_SUB: {
      double x = pop(vm);
      push(vm, pop(vm) - x);
      break;
    }
    case B_MUL: {
      double x = pop(vm);
      push(vm, pop(vm) * x);
      break;
    }
    case B_DIV: {
      double x = pop(vm);
      push(vm, pop(vm) / x);
      break;
    }
    case B_POW: {
      double x = pop(vm);
      push(vm, pow(pop(vm), x));
      break;
    }
    case B_EQ_NUM: {
      double x = pop(vm);
      push_logic(vm, pop(vm) == x);
      break;
    }
    case B_LT_NUM: {
      double x = pop(vm);
      push_logic(vm, pop(vm) < x);
      break;
    }
    case B_GT_NUM: {
      double x = pop(vm);
      push_logic(vm, pop(vm) > x);
      break;
    }
    case B_NE_NUM: {
      double x = pop(vm);
      push_logic(vm, pop(vm) != x);
      break;
    }
    case B_LE_NUM: {
      double x = pop(vm);
      push_logic(vm, pop(vm) <= x);
      break;
    }
    case B_GE_NUM: {
      double x = pop(vm);
      push_logic(vm, pop(vm) >= x);
      break;
    }
    case B_OR: {
      int logic2 = pop_logic(vm);
      int logic1 = pop_logic(vm);
      push(vm, logic1 | logic2);
      break;
    }
    case B_AND: {
      int logic2 = pop_logic(vm);
      int logic1 = pop_logic(vm);
      push(vm, logic1 & logic2);
      break;
    }
    case B_NOT: {
      int logic = pop_logic(vm);
      push(vm, ~logic);
      break;
    }
    // string
    case B_PUSH_STR:
      push_str(vm, i->u.str);
      break;
    case B_SET_SIMPLE_STR:
      set_strvar_name(vm, i->u.name, pop_str(vm));
      break;
    case B_GET_SIMPLE_STR: {
      int j = lookup_strvar_name(vm, i->u.name);
      if (j < 0) {
        if (vm->strict_variables)
          run_error(vm, "Variable not found: %s\n", stringlist_item(vm->env->names, i->u.name));
        push_str(vm, "");
      }
      else
        push_str(vm, vm->env->strvars.vars[j].val);
      break;
    }
    case B_DIM_STR: {
      PAREN_SYMBOL* sym = lookup_paren_name(&vm->env->paren, i->u.param.name);
      if (sym) {
        check_paren_kind(vm, sym, PK_ARRAY);
        if (sym->type != TYPE_STR)
          run_error(vm, "cannot redimension: not a string array: %s\n", stringlist_item(vm->env->names, i->u.param.name));
        redimension_string(vm, sym, i->u.param.params);
      }
      else
        dimension_string(vm, i->u.param.name, i->u.param.params);
      break;
    }
    case B_GET_PAREN_STR: {
      PAREN_SYMBOL* sym = lookup_paren_name(&vm->env->paren, i->u.param.name);
      if (sym == NULL) {
        unsigned indexes[MAX_DIMENSIONS];
        pop_indexes(vm, indexes, i->u.param.params);
        sym = dimension_string_auto(vm, i->u.param.name, i->u.param.params, indexes);
        push_str(vm, "");
        break;
      }
      if (sym->type != TYPE_STR)
        run_error(vm, "string array or function was expected: %s\n", stringlist_item(vm->env->names, i->u.param.name));
      switch (sym->kind) {
        case PK_ARRAY: {
          unsigned indexes[MAX_DIMENSIONS];
          pop_indexes(vm, indexes, i->u.param.params);
          push_str(vm, *string_element(vm, sym->u.strarr, i->u.param.name, i->u.param.params, indexes));
          break;
        }
        case PK_DEF:
          call_def(vm, sym->u.def, i->u.param.name, i->u.param.params);
          break;
        default:
          fatal("internal error: unexpected kind of paren symbol\n");
      }
      break;
    }
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
    case B_CONCAT: {
      char* t = pop_str(vm);
      char* s = pop_str(vm);
      size_t sz = strlen(s) + strlen(t);
      char buf[256];
      if (sz + 1 > sizeof buf)
        run_error(vm, "concatenated string would be too long: %lu characters\n", (unsigned long)sz);
      strcpy(buf, s);
      strcat(buf, t);
      push_str(vm, buf);
      efree(s);
      efree(t);
      break;
    }
    // control flow
    case B_END:
      vm->pc = vm->bc->used;
      return;
    case B_STOP:
      vm->stopped = true;
      return;
    case B_GOTO:
      go_to(vm, i->u.line);
      return;
    case B_GOTRUE:
      if (pop(vm)) {
        go_to(vm, i->u.line);
        return;
      }
      break;
    case B_GOSUB:
      push_return(vm, vm->pc + 1);
      go_to(vm, i->u.line);
      return;
    case B_RETURN:
      pop_return(vm); // pops PC to continue from
      return;
    case B_FOR: {
      if (vm->trace_for)
        dump_for(vm, "FOR");
      int si = find_for(vm, i->u.name);
      if (si >= 0) {
        if (vm->strict_for)
          run_error(vm, "already inside FOR loop controlled by this variable: %s\n", numvar_name(vm, vm->for_stack[si].var));
        assert(vm->for_sp > 0);
        if (si != vm->for_sp - 1) {
          // bring inner loop to top of stack, for NEXT with no variable
          struct for_loop inner = vm->for_stack[si];
          for (unsigned k = si; k < vm->for_sp - 1; k++)
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
      struct for_loop * f = &vm->for_stack[si];
      f->var = lookup_numvar_name(vm, i->u.name);
      if (f->var < 0)
        f->var = insert_numvar_name(vm, i->u.name);
      f->step = pop(vm);
      f->limit = pop(vm);
      vm->env->numvars.vars[f->var].val = pop(vm);
      f->source = vm->source;
      f->source_line = vm->source_line;
      f->bc = vm->bc;
      f->pc = vm->pc;
      if (vm->trace_for)
        dump_for_stack(vm, "final stack");
      break;
    }
    case B_NEXT_VAR: {
      if (vm->trace_for)
        dump_for(vm, "NEXT-VARIABLE");
      if (vm->for_sp == 0)
        run_error(vm, "NEXT without FOR\n");
      int si = vm->for_sp - 1;
      struct for_loop * f = &vm->for_stack[si];
      int var = lookup_numvar_name(vm, i->u.name);
      if (var != f->var) {
        if (vm->strict_for) {
          const char* for_name = numvar_name(vm, f->var);
          const char* next_name = stringlist_item(vm->env->names, i->u.name);
          run_error(vm, "mismatched FOR variable: expecting %s, found %s\n", for_name, next_name);
        }
        si = find_for(vm, i->u.name);
        if (si < 0)
          run_error(vm, "NEXT without FOR: %s\n", stringlist_item(vm->env->names, i->u.name));
        f = &vm->for_stack[si];
      }
      double x = vm->env->numvars.vars[f->var].val + f->step;
      if (f->step > 0 && x > f->limit || f->step < 0 && x < f->limit) {
        // remove FOR stack index si
        assert(vm->for_sp > 0);
        vm->for_sp--;
        for (unsigned k = si; k < vm->for_sp; k++)
          vm->for_stack[k] = vm->for_stack[k + 1];
      }
      else {
        vm->env->numvars.vars[f->var].val = x;
        vm->bc = f->bc;
        vm->pc = f->pc;
        vm->source = f->source;
        vm->source_line = f->source_line;
      }
      if (vm->trace_for)
        dump_for_stack(vm, "final stack");
      break;
    }
    case B_NEXT_IMP: {
      if (vm->trace_for)
        dump_for(vm, "NEXT-IMPLICIT");
      if (vm->for_sp == 0)
        run_error(vm, "NEXT without FOR\n");
      int si = vm->for_sp - 1;
      struct for_loop * f = &vm->for_stack[si];
      double x = vm->env->numvars.vars[f->var].val + f->step;
      if (f->step > 0 && x > f->limit || f->step < 0 && x < f->limit)
        vm->for_sp--;
      else {
        vm->env->numvars.vars[f->var].val = x;
        vm->bc = f->bc;
        vm->pc = f->pc;
        vm->source = f->source;
        vm->source_line = f->source_line;
      }
      if (vm->trace_for)
        dump_for_stack(vm, "final stack");
      break;
    }
    case B_DEF: {
      PAREN_SYMBOL* sym = lookup_paren_name(&vm->env->paren, i->u.param.name);
      if (sym) {
        check_paren_kind(vm, sym, PK_DEF);
        if (!replace_def(sym, i->u.param.params, vm->source_line, vm->bc, vm->pc))
          run_error(vm, "invalid DEF code\n");
      }
      else {
        int type = string_name(stringlist_item(vm->env->names, i->u.param.name)) ? TYPE_STR : TYPE_NUM;
        if (!insert_def(&vm->env->paren, i->u.param.name, type, i->u.param.params, vm->source_line, vm->bc, vm->pc))
          run_error(vm, "invalid DEF code\n");
      }
      do {
        vm->pc++;
      } while (vm->pc < vm->bc->used && vm->bc->inst[vm->pc].op != B_END_DEF);
      break;
    }
    case B_PARAM:
      run_error(vm, "internal error: run into parameter\n");
      break;
    case B_END_DEF:
      end_def(vm);
      break;
    case B_ON_GOTO:
    case B_ON_GOSUB: {
      double x = pop(vm);
      if (x != floor(x))
        run_error(vm, "ON value is invalid: %g\n", x);
      if (x < 1 || x > i->u.count) {
        if (vm->strict_on)
          run_error(vm, "ON value is out of range: %g\n", x);
        vm->pc += i->u.count + 1;
        return;
      }
      unsigned k = vm->pc + (unsigned) x;
      if (k >= vm->bc->used || vm->bc->inst[k].op != B_ON_LINE)
        run_error(vm, "internal error: ON-LINE expected\n");
      if (i->op == B_ON_GOSUB)
        push_return(vm, vm->pc + i->u.count + 1);
      go_to(vm, vm->bc->inst[k].u.line);
      return;
    }
    case B_IF_THEN: // IF ... THEN statements  -- skip to next line if condition false
      if (!pop(vm)) {
        do {
          vm->pc++;
        } while (vm->pc < vm->bc->used && vm->bc->inst[vm->pc].op != B_LINE);
        return;
      }
      break;
    case B_IF_ELSE: // IF ... THEN statements ELSE statements -- skip to ELSE statements if condition false
      if (!pop(vm)) {
        do {
          vm->pc++;
        } while (vm->pc < vm->bc->used && vm->bc->inst[vm->pc].op != B_ELSE);
      }
      break;
    case B_ELSE: // THEN statements ELSE statements -- skip to next line after executing THEN section
      do {
        vm->pc++;
      } while (vm->pc < vm->bc->used && vm->bc->inst[vm->pc].op != B_LINE);
      return;
    // output
    case B_PRINT_LN:
      putchar('\n');
      vm->col = 1;
      break;
    case B_PRINT_SPC: {
      unsigned k = pop_unsigned(vm);
      while (k--)
        putchar(' '), vm->col++;
      break;
    }
    case B_PRINT_TAB: {
      unsigned k = pop_unsigned(vm);
      if (k < vm->col) {
        putchar('\n');
        vm->col = 1;
      }
      while (vm->col < k)
        putchar(' '), vm->col++;
      break;
    }
    case B_PRINT_COMMA:
      do {
        putchar(' '), vm->col++;
      } while (vm->col % TAB_SIZE != 1);
      break;
    case B_PRINT_NUM:
      vm->col += printf(" %g ", pop(vm));
      break;
    case B_PRINT_STR: {
      char* s = pop_str(vm);
      for (const char* S = s; *S; S++) {
        putchar(*S);
        if (*S == '\n')
          vm->col = 1;
        else
          vm->col++;
      }
      efree(s);
      break;
    }
    case B_CLS:
      clear_screen();
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
    case B_INPUT_END: {
      int c;
      while ((c = vm->input[vm->inp]) == ' ' || c == '\t' || c == '\n' || c == '\r')
        vm->inp++;
      if (c != '\0')
        puts("* Extra input was discarded *");
      break;
    }
    case B_INPUT_SEP: {
      int c;
      while ((c = vm->input[vm->inp]) == ' ' || c == '\t')
        vm->inp++;
      if (c == ',') {
        vm->inp++;
        break;
      }
      puts("* More input items are expected *");
      vm->pc = vm->input_pc;
      return;
    }
    case B_INPUT_NUM: {
      double x;
      const char* t = convert(vm->input + vm->inp, &x);
      if (t != NULL && (*t == '\0' || *t == '\n' || *t == ',')) {
        set_numeric_name(vm, i->u.param.name, i->u.param.params, x);
        vm->inp = (int) (t - vm->input);
        break;
      }
      puts("* Invalid input *");
      vm->pc = vm->input_pc;
      return;
    }
    case B_INPUT_STR: {
      const char* s = vm->input + vm->inp;
      int c;
      while ((c = vm->input[vm->inp]) != '\0' && c != '\n' && c != ',')
        vm->inp++;
      vm->input[vm->inp] = '\0';
      char* t = estrdup(s);
      set_string_name(vm, i->u.param.name, i->u.param.params, t);
      vm->input[vm->inp] = c;
      break;
    }
    case B_INPUT_LINE: {
      char* s = strchr(vm->input, '\n');
      if (s)
        *s = '\0';
      s = estrdup(vm->input);
      set_string_name(vm, i->u.param.name, i->u.param.params, s);
      break;
    }
    // inline data
    case B_DATA:
      break;
    case B_READ_NUM: {
      const char* S = find_data(vm);
      double x;
      const char* t = convert(S, &x);
      if (t == NULL || *t != '\0')
        run_error(vm, "numeric data expected: %s\n", S);
      set_numeric_name(vm, i->u.param.name, i->u.param.params, x);
      break;
    }
    case B_READ_STR: {
      const char* S = find_data(vm);
      set_string_name(vm, i->u.param.name, i->u.param.params, estrdup(S));
      break;
    }
    case B_RESTORE:
      vm->data = 0;
      break;
    // builtins
    case B_ASC: {
      char* s = pop_str(vm);
      push(vm, s[0]);
      efree(s);
      break;
    }
    case B_ABS:
      push(vm, fabs(pop(vm)));
      break;
    case B_ATN:
      push(vm, atan(pop(vm)));
      break;
    case B_CHR: {
      double x = pop(vm);
      if (x < 0 || x > 255 || x != floor(x))
        run_error(vm, "invalid character code: %g\n", x);
      char buf[2];
      buf[0] = (char) x;
      buf[1] = '\0';
      push_str(vm, buf);
      break;
    }
    case B_COS:
      push(vm, cos(pop(vm)));
      break;
    case B_EXP:
      push(vm, exp(pop(vm)));
      break;
    case B_INT:
      push(vm, floor(pop(vm)));
      break;
    case B_LEFT: {
      char* s = pop_str(vm);
      unsigned u = pop_unsigned(vm);
      size_t sz = strlen(s);
      if (u > sz)
        u = (unsigned long)sz;
      char buf[256];
      if (u + 1 > sizeof buf)
        run_error(vm, STRING_TOO_LONG);
      strncpy(buf, s, u);
      buf[u] = '\0';
      push_str(vm, buf);
      efree(s);
      break;
    }
    case B_LEN: {
      char* s = pop_str(vm);
      push(vm, (double) strlen(s));
      efree(s);
      break;
    }
    case B_LOG: {
      double x = pop(vm);
      if (x <= 0)
        run_error(vm, "invalid logarithm\n");
      push(vm, log(x));
      break;
    }
    case B_MID3: {
      char* s = pop_str(vm);
      unsigned v = pop_unsigned(vm);
      unsigned u = pop_unsigned(vm);
      size_t sz = strlen(s);
      if (u < 1 || u > sz)
        run_error(vm, "string index out of range\n");
      if (v > sz - u + 1)
        v = (unsigned long)(sz - u + 1);
      char buf[256];
      if (v + 1 > sizeof buf)
        run_error(vm, STRING_TOO_LONG);
      strncpy(buf, s + u - 1, v);
      buf[v] = '\0';
      push_str(vm, buf);
      efree(s);
      break;
    }
    case B_STR: {
      double x = pop(vm);
      char buf[64];
      sprintf(buf, "%g", x);
      push_str(vm, buf);
      break;
    }
    case B_RIGHT: {
      char* s = pop_str(vm);
      unsigned u = pop_unsigned(vm);
      size_t sz = strlen(s);
      if (u > sz)
        u = (unsigned long)sz;
      char buf[256];
      if (u + 1 > sizeof buf)
        run_error(vm, STRING_TOO_LONG);
      strncpy(buf, s + sz - u, u);
      buf[u] = '\0';
      push_str(vm, buf);
      efree(s);
      break;
    }
    case B_RND: {
      pop(vm); // dummy argument
      double x;
      do {
        x = (double) rand() / RAND_MAX;
      } while (x >= 1);
      push(vm, x);
      break;
    }
    case B_SGN: {
      double x = pop(vm);
      if (x < 0)
        x = -1;
      else if (x > 0)
        x = 1;
      push(vm, x);
      break;
    }
    case B_SIN:
      push(vm, sin(pop(vm)));
      break;
    case B_SQR:
      push(vm, sqrt(pop(vm)));
      break;
    case B_TAN:
      push(vm, tan(pop(vm)));
      break;
    case B_VAL: {
      char* s = pop_str(vm);
      double x;
      const char* t = convert(s, &x);
      if (t == NULL || *t != '\0')
        run_error(vm, "invalid number: %s\n", s);
      efree(s);
      push(vm, x);
      break;
    }
    // unknown opcode
    default:
      fputs("UNKNOWN OPCODE:\n", stderr);
      print_binst(vm->bc, vm->pc, vm->source, vm->env->names, stderr);
      run_error(vm, "unknown opcode: %u\n", i->op);
  }
  vm->pc++;
}

static char* convert(const char* s, double *val) {
  while (*s == ' ' || *s == '\t')
    s++;

  if (isdigit(*s) || *s == '.' || *s == '-' || *s == '+') {
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
  if (vm->fn.bc)
    run_error(vm, "nested user-defined function calls are not allowed\n");

  if (params != def->params)
    run_error(vm, "unexpected number of parameters: %s: expected %u, received %u\n", stringlist_item(vm->env->names, name), def->params, params);

  if (vm->pc + params >= vm->bc->used)
    run_error(vm, "program corrupt: missing parameters: %s\n", stringlist_item(vm->env->names, name));

  vm->fn.bc = vm->bc;
  vm->fn.pc = vm->pc;
  vm->fn.source = vm->source;
  vm->fn.source_line = vm->source_line;
  vm->fn.numvars_count = vm->env->numvars.count;

  vm->bc = def->code;
  vm->pc = 0;
  vm->source = NULL;
  vm->source_line = def->source_line;

  // top of stack is final parameter
  for ( ; params > 0; params--) {
    BINST* p = vm->bc->inst + vm->pc + params;
    if (p->op != B_PARAM) {
      print_binst(vm->bc, vm->pc, vm->source, vm->env->names, stderr);
      run_error(vm, "program corrupt: parameter expected\n");
    }
    int var = insert_numvar_name(vm, p->u.name);
    vm->env->numvars.vars[var].val = pop(vm);
  }

  vm->pc += def->params;
}

static void end_def(VM* vm) {
  if (vm->fn.bc == NULL)
    run_error(vm, "unexpected END DEF\n");
  vm->bc = vm->fn.bc;
  vm->pc = vm->fn.pc;
  vm->source = vm->fn.source;
  vm->source_line = vm->fn.source_line;
  vm->env->numvars.count = vm->fn.numvars_count;
  vm->fn.bc = NULL;
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
    if (vm->env->numvars.vars[var].name == name)
      return j;
  }
  return -1;
}

static void dump_for(VM* vm, const char* tag) {
  printf("[%s]\n", tag);
  printf("-- line: %u %s\n", source_linenum(vm->source, vm->source_line), source_text(vm->source, vm->source_line));
  dump_for_stack(vm, "initial stack");
}

static void dump_for_stack(VM* vm, const char* tag) {
  printf("-- %s: ", tag);
  if (vm->for_sp == 0)
    fputs("empty", stdout);
  else {
    for (unsigned j = vm->for_sp; j > 0; j--) {
      int var = vm->for_stack[j-1].var;
      printf("%s = %g, %g; ", numvar_name(vm, var), vm->env->numvars.vars[var].val, vm->for_stack[j-1].limit);
    }
  }
  putc('\n', stdout);
}
