// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

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

// Everything required to specify a piece of code to run:
// may be stored program or immediate code.
typedef struct {
  SOURCE* source;
  BCODE* bcode;
  BCODE_INDEX* index;
} CODE;

static void deinit_code(CODE* code) {
  assert(code != NULL);
  delete_bcode_index(code->index);
  delete_bcode(code->bcode);
  delete_source(code->source);
}

// state of code being run: the code and a position in it
typedef struct {
  const CODE* code;
  unsigned source_line;
  unsigned pc;
} CODE_STATE;

static void clear_code_state(CODE_STATE* cs) {
  cs->code = NULL;
  cs->source_line = 0;
  cs->pc = 0;
}

struct vm {
  CODE stored_program;
  CODE immediate_code;
  CODE def_code; // does not own its bcode: points to DEF code temporarily
  CODE_STATE code_state;
  ENV* env;
  double stack[MAX_NUM_STACK];
  char* strstack[MAX_STR_STACK];
  CODE_STATE retstack[MAX_RETURN_STACK];
  bool stopped;
  unsigned sp;
  unsigned ssp;
  unsigned rsp;
  unsigned col;
  // FOR
  struct for_loop {
    CODE_STATE code_state;
    int var;
    double step;
    double limit;
  } for_stack[MAX_FOR];
  unsigned for_sp;
  // DATA
  unsigned data;
  // FN
  struct {
    CODE_STATE code_state;
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
  bool trace_log;
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

VM* new_vm(bool keywords_anywhere, bool trace_basic, bool trace_for, bool trace_log) {
  VM* vm = ecalloc(1, sizeof *vm);
  vm->env = new_environment_with_builtins();
  vm->col = 1;
  vm->keywords_anywhere = keywords_anywhere;
  vm->trace_basic = trace_basic;
  vm->trace_for = trace_for;
  vm->trace_log = trace_log;
  vm->input_prompt = true;
  return vm;
}

static void clear_string_stack(VM*);

void delete_vm(VM* vm) {
  if (vm) {
    clear_string_stack(vm);
    deinit_code(&vm->stored_program);
    deinit_code(&vm->immediate_code);
    delete_environment(vm->env);
    efree(vm);
  }
}

static void clear_string_stack(VM* vm) {
  while (vm->ssp > 0) {
    vm->ssp--;
    efree(vm->strstack[vm->ssp]);
  }
}

// Reset control state so that no GOSUB, FOR, function call is in progress.
static void reset_control_state(VM* vm) {
  clear_string_stack(vm);
  clear_code_state(&vm->code_state);
  clear_code_state(&vm->fn.code_state);
  vm->stopped = false;
  vm->sp = 0;
  vm->ssp = 0;
  vm->rsp = 0;
  vm->col = 1;
  vm->for_sp = 0;
  vm->data = 0;
  vm->inp = -1;
  vm->input_pc = 0;
}

// Flag stored program source as changed and compiled program as out of date.
// Do not clear environment.
static void stored_program_changed(VM* vm) {
  if (vm->stored_program.index) {
    delete_bcode_index(vm->stored_program.index);
    vm->stored_program.index = NULL;
  }
  if (vm->stored_program.bcode) {
    delete_bcode(vm->stored_program.bcode);
    vm->stored_program.bcode = NULL;
  }
}

// Recompile the stored program if its source has changed since last compilation.
// Return false on compilation error, true otherwise.
static bool ensure_program_compiled(VM* vm) {
  if (vm->stored_program.source != NULL && vm->stored_program.bcode == NULL) {
    assert(vm->stored_program.index == NULL);
    if (vm->verbose)
      puts("Compiling...");
    vm->stored_program.bcode = parse_source(vm->stored_program.source, vm->env->names, vm->keywords_anywhere);
    if (vm->stored_program.bcode == NULL)
      return false;
    vm->stored_program.index = bcode_index(vm->stored_program.bcode, vm->stored_program.source);
    reset_control_state(vm);
  }

  return true;
}

void vm_new_program(VM* vm) {
  assert(vm != NULL);
  if (vm->stored_program.source)
    clear_source(vm->stored_program.source);
  stored_program_changed(vm);
}

// If a source line exists with the given line number, delete it,
// and flag the program as changed since the last compilation.
void vm_delete_source_line(VM* vm, unsigned num) {
  assert(vm != NULL);
  if (vm->stored_program.source) {
    unsigned i;
    if (find_source_linenum(vm->stored_program.source, num, &i)) {
      delete_source_line(vm->stored_program.source, i);
      stored_program_changed(vm);
    }
  }
}

// Add or replace stored program source line,
// first creating stored program SOURCE object if required.
void vm_enter_source_line(VM* vm, unsigned num, const char* text) {
  if (vm->stored_program.source == NULL)
    vm->stored_program.source = new_source(NULL);
  enter_source_line(vm->stored_program.source, num, text);
  stored_program_changed(vm);
}

bool vm_save_source(VM* vm, const char* name) {
  if (vm->stored_program.source == NULL)
    return false;
  return save_source_file(vm->stored_program.source, name);
}

bool vm_load_source(VM* vm, const char* name) {
  SOURCE* source = load_source_file(name); // TODO: errors should not terminate the interpreter
  if (source == NULL)
    return false;

  if (vm->stored_program.source)
    delete_source(vm->stored_program.source);
  vm->stored_program.source = source;
  stored_program_changed(vm);
  return true;
}

const SOURCE* vm_stored_source(VM* vm) {
  return vm->stored_program.source;
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

static void run(VM*);

void run_program(VM* vm) {
  assert(vm != NULL);

  if (vm->stored_program.source == NULL)
    return;

  if (ensure_program_compiled(vm)) {
    reset_control_state(vm);
    vm->code_state.code = &vm->stored_program;
    vm->code_state.source_line = 0;
    vm->code_state.pc = 0;
    run(vm);
  }
}

static bool immediate_state(VM*);

void run_immediate(VM* vm, const char* line) {
  assert(vm != NULL);
  assert(line != NULL);

  deinit_code(&vm->immediate_code);

  vm->immediate_code.source = wrap_source_text(line);
  assert(vm->immediate_code.source != NULL);

  vm->immediate_code.bcode = parse_source(vm->immediate_code.source, vm->env->names, /*keywords_anywhere*/ false);
  if (vm->immediate_code.bcode == NULL)
    return;

  vm->immediate_code.index = bcode_index(vm->immediate_code.bcode, vm->immediate_code.source);

  if (ensure_program_compiled(vm)) {
    vm->code_state.code = &vm->immediate_code;
    run(vm);
    if (immediate_state(vm))
      reset_control_state(vm);
  }
}

// determine whether control state refers to immediate-mode code
// rather than just program code
static bool immediate_state(VM* vm) {
  for (unsigned i = 0; i < vm->rsp; i++) {
    assert(vm->retstack[i].code != NULL);
    if (vm->retstack[i].code != &vm->stored_program)
      return true;
  }

  for (unsigned i = 0; i < vm->for_sp; i++) {
    assert(vm->for_stack[i].code_state.code != NULL);
    if (vm->for_stack[i].code_state.code != &vm->stored_program)
      return true;
  }

  if (vm->fn.code_state.code != NULL && vm->fn.code_state.code != &vm->stored_program)
    return true;

  return false;
}

static void execute(VM*);
static void report_for_in_progress(VM*);

// Run the currently selected code from the beginning.
static void run(VM* vm) {
  assert(vm->code_state.code != NULL);
  vm->code_state.source_line = 0;
  vm->code_state.pc = 0;

  vm->stopped = false;
  trap_interrupt();
  if (setjmp(vm->errjmp) == 0) {
    while (vm->code_state.pc < vm->code_state.code->bcode->used && !vm->stopped && !interrupted)
      execute(vm);
  }
  untrap_interrupt();

  if (interrupted)
    puts("Break");
  else if (vm->stopped) {
    print_source_line(vm->code_state.code->source, vm->code_state.source_line, stdout);
    putchar('\n');
    puts("Stopped");
  }
  else {
    if (vm->strict_for && vm->for_sp != 0)
      report_for_in_progress(vm);
  }
}

static const char* numvar_name(VM*, unsigned var);

static void report_for_in_progress(VM* vm) {
  assert(vm->for_sp != 0);
  struct for_loop * f = &vm->for_stack[vm->for_sp-1];
  const char* name = numvar_name(vm, f->var);
  fprintf(stderr, "FOR without NEXT: %s\n", name);
  print_source_line(f->code_state.code->source, f->code_state.source_line, stderr);
  putc('\n', stderr);
}

static void run_error(VM* vm, const char* fmt, ...) {
  fputs("Runtime error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  if (vm->code_state.code && vm->code_state.code->source) {
    print_source_line(vm->code_state.code->source, vm->code_state.source_line, stderr);
    putc('\n', stderr);
  }

  longjmp(vm->errjmp, 1);
}

static const char STRING_TOO_LONG[] = "string too long\n";

// Declare numeric stack functions
static void push(VM*, double num);
static double pop(VM*);
static void push_logic(VM*, int);
static int pop_logic(VM*);
static unsigned pop_unsigned(VM*);
static void pop_indexes(VM*, unsigned* indexes, unsigned dimensions, const char* name);

// Declare string stack functions
static void push_str(VM*, const char*);
static char* pop_str(VM*);
static int compare_strings(VM*);

// Simple variables
static int insert_numvar_name(VM* vm, unsigned name);
static int insert_strvar_name(VM* vm, unsigned name);
static void set_numvar_name(VM*, unsigned name, double value);
static void set_strvar_name(VM*, unsigned name, char* s);

// Parenthesised arrays and functions
static void check_paren_kind(VM*, const PAREN_SYMBOL*, int kind);

static void dimension_numeric(VM*, unsigned name, unsigned params);
static PAREN_SYMBOL* dimension_numeric_auto(VM*, unsigned name, unsigned params, const unsigned* indexes);
static void redimension_numeric(VM*, PAREN_SYMBOL*, unsigned params);
static double* numeric_element(VM*, struct numeric_array *, unsigned name, unsigned dimensions, const unsigned* indexes);
static void set_numeric_element_name(VM*, unsigned name, unsigned dimensions, double val);

static void dimension_string(VM*, unsigned name, unsigned dimensions);
static PAREN_SYMBOL* dimension_string_auto(VM*, unsigned name, unsigned short dimensions, const unsigned* indexes);
static void redimension_string(VM*, PAREN_SYMBOL*, unsigned dimensions);
static char* * string_element(VM*, struct string_array * p, unsigned name, unsigned dimensions, const unsigned *indexes);
static void set_string_element(VM* vm, struct string_array * p, unsigned name, unsigned dimensions, const unsigned* indexes, char* val);
static void set_string_element_name(VM*, unsigned name, unsigned dimensions, char* val);

// Simple or parenthesised
static void set_numeric_name(VM*, unsigned name, unsigned dimensions, double val);
static void set_string_name(VM*, unsigned name, unsigned dimensions, char* val);

// Number conversion
static char* convert(const char* string, double *val);

// Bcode locations
static unsigned find_basic_line(VM*, unsigned basic_line);
static const char* find_data(VM*);

// Control flow
static void go_to_basic_line(VM*, unsigned basic_line);
static void push_return(VM*, unsigned return_pc);
static void pop_return(VM*);
static int find_for(VM*, unsigned name);
static void call_def(VM*, const struct def *, unsigned name, unsigned params);
static void end_def(VM*);

// Debugging
static void print_stack(const VM*);
static void dump_for(VM*, const char* tag);
static void dump_for_stack(VM*, const char* tag);
static void dump_numvars(const VM*, FILE*);

static void execute(VM* vm) {
  assert(vm->code_state.pc < vm->code_state.code->bcode->used);

  const BINST* const i = vm->code_state.code->bcode->inst + vm->code_state.pc;

  switch (i->op) {
    case B_NOP:
      break;
    // source
    case B_SOURCE_LINE:
      vm->code_state.source_line = i->u.source_line;
      if (vm->trace_basic) {
        printf("[%u]", source_linenum(vm->code_state.code->source, i->u.source_line));
        fflush(stdout);
      }
      if (vm->trace_log) {
        print_source_line(vm->code_state.code->source, vm->code_state.source_line, stderr);
        putc('\n', stderr);
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
    case B_POP_NUM:
      pop(vm);
      break;
    case B_GET_SIMPLE_NUM: {
      int j = env_lookup_numvar(vm->env, i->u.name);
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
      const char* name = stringlist_item(vm->env->names, i->u.param.name);
      PAREN_SYMBOL* sym = lookup_paren_name(&vm->env->paren, i->u.param.name);
      if (sym == NULL) {
        unsigned indexes[MAX_DIMENSIONS];
        pop_indexes(vm, indexes, i->u.param.params, name);
        sym = dimension_numeric_auto(vm, i->u.param.name, i->u.param.params, indexes);
        push(vm, 0);
        break;
      }
      if (sym->type != TYPE_NUM)
        run_error(vm, "numeric array or function was expected: %s\n", name);
      switch (sym->kind) {
        case PK_ARRAY: {
          unsigned indexes[MAX_DIMENSIONS];
          pop_indexes(vm, indexes, i->u.param.params, name);
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
    case B_POP_STR:
      efree(pop_str(vm));
      break;
    case B_SET_SIMPLE_STR:
      set_strvar_name(vm, i->u.name, pop_str(vm));
      break;
    case B_GET_SIMPLE_STR: {
      int j = env_lookup_strvar(vm->env, i->u.name);
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
      const char* name = stringlist_item(vm->env->names, i->u.param.name);
      PAREN_SYMBOL* sym = lookup_paren_name(&vm->env->paren, i->u.param.name);
      if (sym == NULL) {
        unsigned indexes[MAX_DIMENSIONS];
        pop_indexes(vm, indexes, i->u.param.params, name);
        sym = dimension_string_auto(vm, i->u.param.name, i->u.param.params, indexes);
        push_str(vm, "");
        break;
      }
      if (sym->type != TYPE_STR)
        run_error(vm, "string array or function was expected: %s\n", name);
      switch (sym->kind) {
        case PK_ARRAY: {
          unsigned indexes[MAX_DIMENSIONS];
          pop_indexes(vm, indexes, i->u.param.params, name);
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
      vm->code_state.pc = vm->code_state.code->bcode->used;
      return;
    case B_STOP:
      vm->stopped = true;
      return;
    case B_GOTO:
      go_to_basic_line(vm, i->u.basic_line.lineno);
      return;
    case B_GOTRUE:
      if (pop(vm)) {
        go_to_basic_line(vm, i->u.basic_line.lineno);
        return;
      }
      break;
    case B_GOSUB:
      push_return(vm, vm->code_state.pc + 1);
      go_to_basic_line(vm, i->u.basic_line.lineno);
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
      f->code_state = vm->code_state;
      f->var = env_lookup_numvar(vm->env, i->u.name);
      if (f->var < 0)
        f->var = insert_numvar_name(vm, i->u.name);
      f->step = pop(vm);
      f->limit = pop(vm);
      vm->env->numvars.vars[f->var].val = pop(vm);
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
      int var = env_lookup_numvar(vm->env, i->u.name);
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
        vm->code_state = f->code_state;
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
        vm->code_state = f->code_state;
      }
      if (vm->trace_for)
        dump_for_stack(vm, "final stack");
      break;
    }
    case B_DEF: {
      PAREN_SYMBOL* sym = lookup_paren_name(&vm->env->paren, i->u.param.name);
      if (sym) {
        check_paren_kind(vm, sym, PK_DEF);
        if (!replace_def(sym, i->u.param.params, vm->code_state.source_line, vm->code_state.code->bcode, vm->code_state.pc))
          run_error(vm, "invalid DEF code\n");
      }
      else {
        int type = string_name(stringlist_item(vm->env->names, i->u.param.name)) ? TYPE_STR : TYPE_NUM;
        if (!insert_def(&vm->env->paren, i->u.param.name, type, i->u.param.params, vm->code_state.source_line, vm->code_state.code->bcode, vm->code_state.pc))
          run_error(vm, "invalid DEF code\n");
      }
      do {
        vm->code_state.pc++;
      } while (vm->code_state.pc < vm->code_state.code->bcode->used && vm->code_state.code->bcode->inst[vm->code_state.pc].op != B_END_DEF);
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
        vm->code_state.pc += i->u.count + 1;
        return;
      }
      unsigned k = vm->code_state.pc + (unsigned) x;
      if (k >= vm->code_state.code->bcode->used || vm->code_state.code->bcode->inst[k].op != B_ON_LINE)
        run_error(vm, "internal error: ON-LINE expected\n");
      if (i->op == B_ON_GOSUB)
        push_return(vm, vm->code_state.pc + i->u.count + 1);
      go_to_basic_line(vm, vm->code_state.code->bcode->inst[k].u.basic_line.lineno);
      return;
    }
    case B_IF_THEN: // IF ... THEN statements  -- skip to next line if condition false
      if (!pop(vm)) {
        do {
          vm->code_state.pc++;
        } while (vm->code_state.pc < vm->code_state.code->bcode->used && vm->code_state.code->bcode->inst[vm->code_state.pc].op != B_SOURCE_LINE);
        return;
      }
      break;
    case B_IF_ELSE: // IF ... THEN statements ELSE statements -- skip to ELSE statements if condition false
      if (!pop(vm)) {
        do {
          vm->code_state.pc++;
        } while (vm->code_state.pc < vm->code_state.code->bcode->used && vm->code_state.code->bcode->inst[vm->code_state.pc].op != B_ELSE);
      }
      break;
    case B_ELSE: // THEN statements ELSE statements -- skip to next line after executing THEN section
      do {
        vm->code_state.pc++;
      } while (vm->code_state.pc < vm->code_state.code->bcode->used && vm->code_state.code->bcode->inst[vm->code_state.pc].op != B_SOURCE_LINE);
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
      fflush(stdout);
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
      fflush(stdout);
      break;
    }
    case B_PRINT_COMMA:
      do {
        putchar(' '), vm->col++;
      } while (vm->col % TAB_SIZE != 1);
      fflush(stdout);
      break;
    case B_PRINT_NUM:
      vm->col += printf(" %g ", pop(vm));
      fflush(stdout);
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
      fflush(stdout);
      break;
    }
    case B_CLS:
      clear_screen();
      vm->col = 1;
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
      vm->input_pc = vm->code_state.pc;
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
      vm->code_state.pc = vm->input_pc;
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
      vm->code_state.pc = vm->input_pc;
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
    case B_RESTORE_LINE:
      vm->data = find_basic_line(vm, i->u.basic_line.lineno);
      break;
    // random
    case B_RAND:
      srand((unsigned)time(NULL));
      break;
    case B_SEED:
      srand(pop_unsigned(vm));
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
    case B_INKEY:
#if HAS_KBHIT && HAS_GETCH
      if (_kbhit()) {
        char buf[2];
        buf[0] = _getch();
        buf[1] = '\0';
        push_str(vm, buf);
      }
      else
        push_str(vm, "");
#else
      run_error(vm, "INKEY$ is not supported\n");
#endif
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
    case B_TIME_STR: {
      time_t t = time(NULL);
      struct tm * tm = localtime(&t);
      char buf[12];
      sprintf(buf, "%02u:%02u:%02u", tm->tm_hour, tm->tm_min, tm->tm_sec);
      push_str(vm, buf);
      break;
    }
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
      print_binst(vm->code_state.code->bcode, vm->code_state.pc, vm->code_state.code->source, vm->env->names, stderr);
      run_error(vm, "unknown opcode: %u\n", i->op);
  }
  vm->code_state.pc++;
}

static void push(VM* vm, double num) {
  if (vm->sp >= MAX_NUM_STACK)
    run_error(vm, "numeric stack overflow\n");
  vm->stack[vm->sp++] = num;

  if (vm->trace_log)
    print_stack(vm);
}

static double pop(VM* vm) {
  if (vm->sp == 0)
    run_error(vm, "numeric stack empty\n");
  vm->sp--;

  if (vm->trace_log)
    print_stack(vm);

  return vm->stack[vm->sp];
}

static void push_logic(VM* vm, int n) {
  push(vm, n ? -1 : 0);
}

static int pop_logic(VM* vm) {
  double x = pop(vm);
  if (x < INT_MIN || x > INT_MAX || x != floor(x))
    run_error(vm, "invalid logical value: %g\n", x);
  return (int) x;
}

static unsigned pop_unsigned(VM* vm) {
  double x = pop(vm);
  if (x < 0 || floor(x) != x)
    run_error(vm, "non-negative integer was expected: %g\n", x);
  if (x > (unsigned)(-1))
    run_error(vm, "out of range: %g\n", x);
  return (unsigned) x;
}

static unsigned pop_index(VM* vm, const char* name) {
  double x = pop(vm);
  if (x < 0 || floor(x) != x)
    run_error(vm, "non-negative integer was expected to index %s: %g\n", name, x);
  if (x > (unsigned)(-1))
    run_error(vm, "out of integer range to index %s: %g\n", name, x);
  return (unsigned) x;
}

static void pop_indexes(VM* vm, unsigned* indexes, unsigned dimensions, const char* name) {
  assert(dimensions <= MAX_DIMENSIONS);
  for (unsigned i = 0; i < dimensions; i++)
    indexes[i] = pop_index(vm, name);
}

static void push_str(VM* vm, const char* s) {
  if (vm->ssp >= MAX_STR_STACK)
    run_error(vm, "string stack overflow\n");
  vm->strstack[vm->ssp++] = estrdup(s ? s : "");
}

static char* pop_str(VM* vm) {
  if (vm->ssp == 0)
    run_error(vm, "string stack empty\n");
  vm->ssp--;
  return vm->strstack[vm->ssp];
}

static int compare_strings(VM* vm) {
  char* t = pop_str(vm);
  char* s = pop_str(vm);
  int r = strcmp(s, t);
  efree(s);
  efree(t);
  return r;
}

static void check_name(VM* vm, unsigned name) {
  if (name >= stringlist_count(vm->env->names))
    run_error(vm, "internal error: name index out of range\n");
}

static const char* numvar_name(VM* vm, unsigned var) {
  if (var >= vm->env->numvars.count)
    run_error(vm, "Internal error: numvar_name: variable number out of range\n");
  return stringlist_item(vm->env->names, vm->env->numvars.vars[var].name);
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

static int insert_strvar_name(VM* vm, unsigned name) {
  check_name(vm, name);

  if (vm->env->strvars.count >= MAX_STR_VAR)
    run_error(vm, "too many string variables\n");

  int i = vm->env->strvars.count++;
  vm->env->strvars.vars[i].name = name;
  vm->env->strvars.vars[i].val = NULL;
  return i;
}

static void set_numvar_name(VM* vm, unsigned name, double val) {
  int j = env_lookup_numvar(vm->env, name);
  if (j < 0)
    j = insert_numvar_name(vm, name);
  vm->env->numvars.vars[j].val = val;
}

static void set_strvar_name(VM* vm, unsigned name, char* s) {
  int j = env_lookup_strvar(vm->env, name);
  if (j < 0)
    j = insert_strvar_name(vm, name);
  efree(vm->env->strvars.vars[j].val);
  vm->env->strvars.vars[j].val = s;
}

static void check_paren_kind(VM* vm, const PAREN_SYMBOL* sym, int kind) {
  assert(sym != NULL);

  if (sym->kind != kind)
    run_error(vm, "name is %s, not %s: %s\n", paren_kind(sym->kind), paren_kind(kind), stringlist_item(vm->env->names, sym->name));
}

static void dimension_numeric(VM* vm, unsigned namei, unsigned dimensions) {
  const char* name = stringlist_item(vm->env->names, namei);
  unsigned max[MAX_DIMENSIONS];
  pop_indexes(vm, max, dimensions, name);
  if (insert_numeric_array(&vm->env->paren, namei, vm->array_base, dimensions, max) == NULL)
    run_error(vm, "invalid dimensions: %s\n", name);
}

static PAREN_SYMBOL* dimension_numeric_auto(VM* vm, unsigned name, unsigned dimensions, const unsigned* indexes) {
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

  const char* name = stringlist_item(vm->env->names, sym->name);
  unsigned max[MAX_DIMENSIONS];
  pop_indexes(vm, max, dimensions, name);
  if (!replace_numeric_array(sym, vm->array_base, dimensions, max))
    run_error(vm, "invalid dimensions: %s\n", name);
}

static double* numeric_element(VM* vm, struct numeric_array * p, unsigned name, unsigned dimensions, const unsigned* indexes) {
  double* addr = NULL;
  if (!compute_numeric_element(p, dimensions, indexes, &addr))
    run_error(vm, "array indexes invalid or out of range: %s\n", stringlist_item(vm->env->names, name));
  return addr;
}

static void set_numeric_element_name(VM* vm, unsigned namei, unsigned dimensions, double val) {
  const char* name = stringlist_item(vm->env->names, namei);
  unsigned indexes[MAX_DIMENSIONS];
  pop_indexes(vm, indexes, dimensions, name);

  PAREN_SYMBOL* sym = lookup_paren_name(&vm->env->paren, namei);
  if (sym == NULL)
    sym = dimension_numeric_auto(vm, namei, dimensions, indexes);
  else if (sym->kind != PK_ARRAY || sym->type != TYPE_NUM)
    run_error(vm, "not a numeric array: %s\n", name);

  *numeric_element(vm, sym->u.numarr, namei, dimensions, indexes) = val;
}

static void dimension_string(VM* vm, unsigned namei, unsigned dimensions) {
  const char* name = stringlist_item(vm->env->names, namei);
  unsigned indexes[MAX_DIMENSIONS];
  pop_indexes(vm, indexes, dimensions, name);
  if (insert_string_array(&vm->env->paren, namei, vm->array_base, dimensions, indexes) == NULL)
    run_error(vm, "invalid dimensions: %s\n", namei);
}

static PAREN_SYMBOL* dimension_string_auto(VM* vm, unsigned name, unsigned short dimensions, const unsigned* indexes) {
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

  const char* name = stringlist_item(vm->env->names, sym->name);
  unsigned max[MAX_DIMENSIONS];
  pop_indexes(vm, max, dimensions, name);
  if (!replace_string_array(sym, vm->array_base, dimensions, max))
    run_error(vm, "invalid dimensions: %s\n", name);
}

static char* * string_element(VM* vm, struct string_array * p, unsigned name, unsigned dimensions, const unsigned *indexes) {
  char* * addr = NULL;
  if (!compute_string_element(p, dimensions, indexes, &addr))
    run_error(vm, "array indexes invalid or out of range: %s\n", stringlist_item(vm->env->names, name));
  assert(addr != NULL);
  return addr;
}

static void set_string_element(VM* vm, struct string_array * p, unsigned name, unsigned dimensions, const unsigned* indexes, char* val) {
  char* *addr = string_element(vm, p, name, dimensions, indexes);
  efree(*addr);
  *addr = val;
}

static void set_string_element_name(VM* vm, unsigned namei, unsigned dimensions, char* val) {
  const char* name = stringlist_item(vm->env->names, namei);
  unsigned indexes[MAX_DIMENSIONS];
  pop_indexes(vm, indexes, dimensions, name);

  PAREN_SYMBOL* sym = lookup_paren_name(&vm->env->paren, namei);
  if (sym == NULL)
    sym = dimension_string_auto(vm, namei, dimensions, indexes);
  if (sym->kind != PK_ARRAY || sym->type != TYPE_STR)
    run_error(vm, "not a string array: %s\n", name);
  set_string_element(vm, sym->u.strarr, namei, dimensions, indexes, val);
}

static void set_numeric_name(VM* vm, unsigned name, unsigned dimensions, double val) {
  if (dimensions == 0)
    set_numvar_name(vm, name, val);
  else
    set_numeric_element_name(vm, name, dimensions, val);
}

static void set_string_name(VM* vm, unsigned name, unsigned dimensions, char* val) {
  if (dimensions == 0)
    set_strvar_name(vm, name, val);
  else
    set_string_element_name(vm, name, dimensions, val);
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

static unsigned find_basic_line(VM* vm, unsigned basic_line) {
  unsigned bcode_line;
  if (!bcode_find_indexed_basic_line(vm->code_state.code->index, basic_line, &bcode_line))
    run_error(vm, "Line not found: %u\n", basic_line);
  return bcode_line;
}

static void go_to_basic_line(VM* vm, unsigned basic_line) {
  vm->code_state.code = &vm->stored_program;
  vm->code_state.pc = find_basic_line(vm, basic_line);
  // vm->code_state.source_line will be set by the LINE command at that PC
}

static const char* find_data(VM* vm) {
  while (vm->data < vm->code_state.code->bcode->used && vm->code_state.code->bcode->inst[vm->data].op != B_DATA)
    vm->data++;

  if (vm->data >= vm->code_state.code->bcode->used)
    run_error(vm, "out of data\n");

  const char* s = vm->code_state.code->bcode->inst[vm->data++].u.str;
  if (s == NULL)
    run_error(vm, "internal error: null data\n");

  return s;
}

static void push_return(VM* vm, unsigned pc_continue) {
  if (vm->rsp >= MAX_RETURN_STACK)
    run_error(vm, "GOSUB is nested too deeply\n");
  vm->retstack[vm->rsp] = vm->code_state;
  vm->retstack[vm->rsp].pc = pc_continue;
  vm->rsp++;
}

static void pop_return(VM* vm) {
  if (vm->rsp == 0)
    run_error(vm, "RETURN without GOSUB\n");
  vm->rsp--;
  vm->code_state = vm->retstack[vm->rsp];
}

static int find_for(VM* vm, unsigned name) {
  for (int j = 0; j < (int)vm->for_sp; j++) {
    int var = vm->for_stack[j].var;
    if (vm->env->numvars.vars[var].name == name)
      return j;
  }
  return -1;
}

static void call_def(VM* vm, const struct def * def, unsigned name, unsigned params) {
  if (vm->fn.code_state.code)
    run_error(vm, "nested user-defined function calls are not allowed\n");

  if (params != def->params)
    run_error(vm, "unexpected number of parameters: %s: expected %u, received %u\n", stringlist_item(vm->env->names, name), def->params, params);

  if (vm->code_state.pc + params >= vm->code_state.code->bcode->used)
    run_error(vm, "program corrupt: missing parameters: %s\n", stringlist_item(vm->env->names, name));

  vm->fn.code_state = vm->code_state;
  vm->fn.numvars_count = vm->env->numvars.count;

  vm->def_code.bcode = def->code;
  assert(vm->def_code.source == NULL);
  assert(vm->def_code.index == NULL);
  vm->code_state.code = &vm->def_code;
  vm->code_state.source_line = def->source_line;
  vm->code_state.pc = 0;

  // top of stack is final parameter
  for ( ; params > 0; params--) {
    BINST* p = def->code->inst + params;
    if (p->op != B_PARAM) {
      print_binst(def->code, params, NULL, vm->env->names, stderr);
      run_error(vm, "program corrupt: parameter expected\n");
    }
    int var = insert_numvar_name(vm, p->u.name);
    vm->env->numvars.vars[var].val = pop(vm);
  }

  vm->code_state.pc += def->params;
}

static void end_def(VM* vm) {
  if (vm->fn.code_state.code == NULL)
    run_error(vm, "unexpected END DEF\n");
  vm->code_state = vm->fn.code_state;
  vm->env->numvars.count = vm->fn.numvars_count;
  vm->fn.code_state.code = NULL;
}

static void print_stack(const VM* vm) {
  fputs("STACK:", stderr);
  for (unsigned i = 0; i < vm->sp; i++)
    fprintf(stderr, " %g", vm->stack[i]);
  putc('\n', stderr);
}

static void dump_for(VM* vm, const char* tag) {
  printf("[%s]\n", tag);
  printf("-- line: %u %s\n", source_linenum(vm->code_state.code->source, vm->code_state.source_line), source_text(vm->code_state.code->source, vm->code_state.source_line));
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

static void dump_numvars(const VM* vm, FILE* fp) {
  for (unsigned i = 0; i < vm->env->numvars.count; i++)
    fprintf(fp, "%s=%g\n", stringlist_item(vm->env->names, vm->env->numvars.vars[i].name), vm->env->numvars.vars[i].val);
}

#ifdef UNIT_TEST

#include "CuTest.h"

static void test_new_vm(CuTest* tc) {
  VM* vm = new_vm(true, true, false, false);
  CuAssertPtrNotNull(tc, vm);

  CuAssertPtrEquals(tc, NULL, vm->stored_program.source);
  CuAssertPtrEquals(tc, NULL, vm->stored_program.bcode);
  CuAssertPtrEquals(tc, NULL, vm->stored_program.index);

  CuAssertPtrEquals(tc, NULL, vm->immediate_code.source);
  CuAssertPtrEquals(tc, NULL, vm->immediate_code.bcode);
  CuAssertPtrEquals(tc, NULL, vm->immediate_code.index);

  CuAssertPtrEquals(tc, NULL, vm->def_code.source);
  CuAssertPtrEquals(tc, NULL, vm->def_code.bcode);
  CuAssertPtrEquals(tc, NULL, vm->def_code.index);

  CuAssertTrue(tc, vm->code_state.code == NULL);
  CuAssertIntEquals(tc, 0, vm->code_state.source_line);
  CuAssertIntEquals(tc, 0, vm->code_state.pc);

  CuAssertPtrNotNull(tc, vm->env);

  CuAssertIntEquals(tc, false, vm->stopped);
  CuAssertIntEquals(tc, 0, vm->sp);
  CuAssertIntEquals(tc, 0, vm->ssp);
  CuAssertIntEquals(tc, 0, vm->rsp);
  CuAssertIntEquals(tc, 1, vm->col);
  CuAssertIntEquals(tc, 0, vm->for_sp);
  CuAssertIntEquals(tc, 0, vm->data);

  CuAssertTrue(tc, vm->fn.code_state.code == NULL);
  CuAssertIntEquals(tc, 0, vm->fn.numvars_count);

  CuAssertIntEquals(tc, 0, vm->input[0]);
  CuAssertIntEquals(tc, 0, vm->inp);
  CuAssertIntEquals(tc, 0, vm->input_pc);

  CuAssertIntEquals(tc, true, vm->keywords_anywhere);
  CuAssertIntEquals(tc, true, vm->trace_basic);
  CuAssertIntEquals(tc, false, vm->trace_for);
  CuAssertIntEquals(tc, false, vm->trace_log);

  CuAssertIntEquals(tc, 0, vm->array_base);

  CuAssertIntEquals(tc, false, vm->strict_dim);
  CuAssertIntEquals(tc, false, vm->strict_for);
  CuAssertIntEquals(tc, false, vm->strict_on);
  CuAssertIntEquals(tc, false, vm->strict_variables);
  CuAssertIntEquals(tc, true, vm->input_prompt);
  CuAssertIntEquals(tc, false, vm->verbose);

  delete_vm(vm);
}

CuSuite* run_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_new_vm);
  return suite;
}

#endif // UNIT_TEST
