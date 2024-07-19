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
#include "symbol.h"
#include "arrays.h"
#include "def.h"
#include "init.h"
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
  code->source = NULL;
  code->bcode = NULL;
  code->index = NULL;
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
  CODE def_code; // does not own its bcode or source: points to DEF code temporarily
  CODE_STATE code_state;
  SYMTAB* st;
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
    SYMID symbol_id;
    double step;
    double limit;
  } for_stack[MAX_FOR];
  unsigned for_sp;
  // DATA
  unsigned program_data;
  unsigned immediate_data;
  // FN
  struct {
    CODE_STATE code_state;
    SYMID param_id;
    bool param_defined;
    double param_val;
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
  vm->st = new_symbol_table();
  init_builtins(vm->st);
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
    delete_symbol_table(vm->st);
    efree(vm);
  }
}

void vm_clear_names(VM* vm) {
  clear_symbol_table_names(vm->st);
  init_builtins(vm->st);
}

void vm_clear_values(VM* vm) {
  clear_symbol_table_values(vm->st);
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
  vm->program_data = 0;
  vm->immediate_data = 0;
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
    clear_symbol_table_names(vm->st);
    init_builtins(vm->st);
    vm->stored_program.bcode = parse_source(vm->stored_program.source, vm->st, vm->keywords_anywhere);
    if (vm->stored_program.bcode == NULL)
      return false;
    vm->stored_program.index = bcode_index(vm->stored_program.bcode, vm->stored_program.source);
    reset_control_state(vm); // resets DATA pointer
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

  // compile stored program before immediate line
  // because ensure_program_compiled might clear symbol table
  if (ensure_program_compiled(vm)) {
    SOURCE* source = wrap_source_text(line);
    vm->immediate_code.bcode = parse_source(source, vm->st, /*keywords_anywhere*/ false);
    if (vm->immediate_code.bcode == NULL) {
      delete_source(source);
      return;
    }
    vm->immediate_code.index = bcode_index(vm->immediate_code.bcode, source);
    vm->immediate_code.source = source;
    vm->immediate_data = 0;
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

static void report_for_in_progress(VM* vm) {
  assert(vm->for_sp != 0);
  struct for_loop * f = &vm->for_stack[vm->for_sp-1];
  SYMBOL* sym = symbol(vm->st, f->symbol_id);
  assert(sym != NULL);
  fprintf(stderr, "FOR without NEXT: %s\n", sym->name);
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

// Numeric variables and arrays
static void get_numeric_simple(VM*, SYMBOL*);
static void set_numeric_simple(VM*, SYMBOL*, double val);

static void dimension_numeric(VM*, SYMBOL*, unsigned ndim, const unsigned max[]);
static void dimension_numeric_auto(VM*, SYMBOL*, unsigned ndim, const unsigned indexes[]);

static void get_numeric_element(VM*, SYMBOL*, unsigned ndim);
static void set_numeric_element(VM*, SYMBOL*, unsigned ndim, double val);

static void set_numeric(VM*, SYMBOL*, unsigned ndim, double val);

// String variables and arrays
static void get_string_simple(VM*, SYMBOL*);
static void set_string_simple(VM*, SYMBOL*, char* val);

static void dimension_string(VM*, SYMBOL*, unsigned ndim, const unsigned max[]);
static void dimension_string_auto(VM*, SYMBOL*, unsigned ndim, const unsigned indexes[]);

static void get_string_element(VM*, SYMBOL*, unsigned ndim);
static void set_string_element(VM*, SYMBOL*, unsigned ndim, char* val);

static void set_string(VM*, SYMBOL*, unsigned ndim, char* val);

// Number conversion
static char* convert(const char* string, double *val);

// Bcode locations
static unsigned find_basic_line(VM*, unsigned basic_line);
static const char* find_data(VM*);

// Control flow
static void go_to_basic_line(VM*, unsigned basic_line);
static void push_return(VM*, unsigned return_pc);
static void pop_return(VM*);
static int find_for(VM*, SYMID);
static void next(VM*, int stack_index);
static void call_def(VM*, SYMBOL*, unsigned params);
static void end_def(VM*);

// Debugging
static void print_stack(const VM*);
static void dump_for(VM*, const char* tag);
static void dump_for_stack(VM*, const char* tag);

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
      vm_clear_values(vm);
      break;
    // number
    case B_PUSH_NUM:
      push(vm, i->u.num);
      break;
    case B_POP_NUM:
      pop(vm);
      break;
    case B_GET_SIMPLE_NUM: {
      SYMBOL* sym = symbol(vm->st, i->u.symbol_id);
      get_numeric_simple(vm, sym);
      break;
    }
    case B_SET_SIMPLE_NUM: {
      SYMBOL* sym = symbol(vm->st, i->u.symbol_id);
      set_numeric_simple(vm, sym, pop(vm));
      break;
    }
    case B_DIM_NUM: {
      SYMBOL* sym = symbol(vm->st, i->u.param.symbol_id);
      assert(sym != NULL && sym->kind == SYM_ARRAY && sym->type == TYPE_NUM);
      if (sym->val.numarr) {
        assert(sym->defined);
        delete_numeric_array(sym->val.numarr);
        sym->val.numarr = NULL;
        sym->defined = false;
      }
      unsigned max[MAX_DIMENSIONS];
      pop_indexes(vm, max, i->u.param.params, sym->name);
      dimension_numeric(vm, sym, i->u.param.params, max);
      break;
    }
    case B_GET_PAREN_NUM: {
      SYMBOL* sym = symbol(vm->st, i->u.param.symbol_id);
      assert(sym != NULL && sym->type == TYPE_NUM);
      // must be a parenthesised kind of symbol; not a builtin,
      // which has its own opcodes; therefore array or user-defined function
      if (sym->kind == SYM_ARRAY)
        get_numeric_element(vm, sym, i->u.param.params);
      else {
        assert(sym->kind == SYM_DEF);
        call_def(vm, sym, i->u.param.params);
      }
      break;
    }
    case B_SET_ARRAY_NUM: {
      SYMBOL* sym = symbol(vm->st, i->u.param.symbol_id);
      set_numeric_element(vm, sym, i->u.param.params, pop(vm));
      break;
    }
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
    case B_SET_SIMPLE_STR: {
      SYMBOL* sym = symbol(vm->st, i->u.symbol_id);
      set_string_simple(vm, sym, pop_str(vm));
      break;
    }
    case B_GET_SIMPLE_STR: {
      SYMBOL* sym = symbol(vm->st, i->u.symbol_id);
      get_string_simple(vm, sym);
      break;
    }
    case B_DIM_STR: {
      SYMBOL* sym = symbol(vm->st, i->u.param.symbol_id);
      assert(sym != NULL && sym->kind == SYM_ARRAY && sym->type == TYPE_STR);
      if (sym->val.strarr) {
        assert(sym->defined);
        delete_string_array(sym->val.strarr);
        sym->val.strarr = NULL;
        sym->defined = false;
      }
      unsigned max[MAX_DIMENSIONS];
      pop_indexes(vm, max, i->u.param.params, sym->name);
      dimension_string(vm, sym, i->u.param.params, max);
      break;
    }
    case B_GET_PAREN_STR: {
      SYMBOL* sym = symbol(vm->st, i->u.param.symbol_id);
      assert(sym != NULL && sym->type == TYPE_STR);
      // must be a parenthesised kind of symbol; not a builtin,
      // which has its own opcodes; therefore array or user-defined function
      if (sym->kind == SYM_ARRAY)
        get_string_element(vm, sym, i->u.param.params);
      else {
        assert(sym->kind == SYM_DEF);
        call_def(vm, sym, i->u.param.params);
      }
      break;
    }
    case B_SET_ARRAY_STR: {
      SYMBOL* sym = symbol(vm->st, i->u.param.symbol_id);
      set_string_element(vm, sym, i->u.param.params, pop_str(vm));
      break;
    }
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
      SYMBOL* sym = symbol(vm->st, i->u.symbol_id);
      assert(sym != NULL && sym->kind == SYM_VARIABLE && sym->type == TYPE_NUM);
      int si = find_for(vm, i->u.symbol_id);
      if (si >= 0) {
        if (vm->strict_for)
          run_error(vm, "already inside FOR loop controlled by this variable: %s\n", sym->name);
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
      f->symbol_id = i->u.symbol_id;
      f->step = pop(vm);
      f->limit = pop(vm);
      sym->val.num = pop(vm);
      sym->defined = true;
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
      if (f->symbol_id != i->u.symbol_id) {
        if (vm->strict_for) {
          const char* for_name = sym_name(vm->st, f->symbol_id);
          const char* next_name = sym_name(vm->st, i->u.symbol_id);
          run_error(vm, "mismatched FOR variable: expecting %s, found %s\n", for_name, next_name);
        }
        si = find_for(vm, i->u.symbol_id);
        if (si < 0)
          run_error(vm, "NEXT without FOR: %s\n", sym_name(vm->st, i->u.symbol_id));
        f = &vm->for_stack[si];
      }
      next(vm, si);
      break;
    }
    case B_NEXT_IMP: {
      if (vm->trace_for)
        dump_for(vm, "NEXT-IMPLICIT");
      if (vm->for_sp == 0)
        run_error(vm, "NEXT without FOR\n");
      next(vm, vm->for_sp - 1);
      break;
    }
    case B_DEF: {
      SYMBOL* sym = symbol(vm->st, i->u.param.symbol_id);
      assert(sym != NULL && sym->kind == SYM_DEF);
      if (sym->val.def) {
        assert(sym->defined);
        delete_def(sym->val.def);
        sym->val.def = NULL;
        sym->defined = false;
      }
      if (i->u.param.params != 1)
        run_error(vm, "unexpected number of parameters: %s\n", sym->name);
      const BCODE* bcode = vm->code_state.code->bcode;
      SOURCE* source = NULL;
      unsigned source_line = 0;
      if (vm->code_state.code == &vm->stored_program) {
        source = vm->stored_program.source;
        source_line = vm->code_state.source_line;
      }
      sym->val.def = new_def(bcode_copy_def(bcode, vm->code_state.pc), source, source_line);
      sym->defined = true;
      do {
        vm->code_state.pc++;
      } while (vm->code_state.pc < bcode->used && bcode->inst[vm->code_state.pc].op != B_END_DEF);
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
        SYMBOL* sym = symbol(vm->st, i->u.param.symbol_id);
        set_numeric(vm, sym, i->u.param.params, x);
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
      vm->input[vm->inp] = c;
      SYMBOL* sym = symbol(vm->st, i->u.param.symbol_id);
      set_string(vm, sym, i->u.param.params, t);
      break;
    }
    case B_INPUT_LINE: {
      char* s = strchr(vm->input, '\n');
      if (s)
        *s = '\0';
      s = estrdup(vm->input);
      SYMBOL* sym = symbol(vm->st, i->u.param.symbol_id);
      set_string(vm, sym, i->u.param.params, s);
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
      SYMBOL* sym = symbol(vm->st, i->u.param.symbol_id);
      set_numeric(vm, sym, i->u.param.params, x);
      break;
    }
    case B_READ_STR: {
      const char* S = find_data(vm);
      SYMBOL* sym = symbol(vm->st, i->u.param.symbol_id);
      set_string(vm, sym, i->u.param.params, estrdup(S));
      break;
    }
    case B_RESTORE:
      vm->program_data = 0;
      vm->immediate_data = 0;
      break;
    case B_RESTORE_LINE:
      vm->program_data = find_basic_line(vm, i->u.basic_line.lineno);
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
      print_binst(i, vm->code_state.pc, vm->code_state.code->source, vm->st, stderr);
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

static void get_numeric_simple(VM* vm, SYMBOL* sym) {
  assert(sym != NULL && sym->kind == SYM_VARIABLE && sym->type == TYPE_NUM);

  if (!sym->defined) {
    if (vm->strict_variables)
      run_error(vm, "Variable not found: %s\n", sym->name);
    sym->val.num = 0;
    sym->defined = true;
  }
  push(vm, sym->val.num);
}

static void set_numeric_simple(VM* vm, SYMBOL* sym, double val) {
  assert(sym != NULL && sym->kind == SYM_VARIABLE && sym->type == TYPE_NUM);

  sym->val.num = val;
  sym->defined = true;
}

static void dimension_numeric(VM* vm, SYMBOL* sym, unsigned ndim, const unsigned max[]) {
  assert(sym != NULL && sym->kind == SYM_ARRAY && sym->type == TYPE_NUM);

  sym->val.numarr = new_numeric_array(vm->array_base, ndim, max);
  if (sym->val.numarr == NULL) {
    sym->defined = false;
    run_error(vm, "invalid dimensions: %s\n", sym->name);
  }
  sym->defined = true;
}

// dimension arrays automatically to 10 or the indexes used
static void dimension_numeric_auto(VM* vm, SYMBOL* sym, unsigned ndim, const unsigned* indexes) {
  assert(sym != NULL && sym->val.numarr == NULL);

  unsigned max[MAX_DIMENSIONS];
  for (unsigned j = 0; j < ndim; j++)
    max[j] = indexes[j] < 10 ? 10 : indexes[j];

  dimension_numeric(vm, sym, ndim, max);
}

static double* numeric_element(VM* vm, struct numeric_array * p, const char* name, unsigned dimensions, const unsigned* indexes) {
  double* addr = NULL;
  if (!compute_numeric_element(p, dimensions, indexes, &addr))
    run_error(vm, "array indexes invalid or out of range: %s\n", name);
  return addr;
}

static void get_numeric_element(VM* vm, SYMBOL* sym, unsigned ndim) {
  assert(sym != NULL && sym->kind == SYM_ARRAY && sym->type == TYPE_NUM);

  unsigned indexes[MAX_DIMENSIONS];
  pop_indexes(vm, indexes, ndim, sym->name);

  if (!sym->defined)
    dimension_numeric_auto(vm, sym, ndim, indexes);

  assert(sym->val.numarr != NULL);
  push(vm, *numeric_element(vm, sym->val.numarr, sym->name, ndim, indexes));
}

static void set_numeric_element(VM* vm, SYMBOL* sym, unsigned ndim, double val) {
  assert(sym != NULL && sym->kind == SYM_ARRAY && sym->type == TYPE_NUM);

  unsigned indexes[MAX_DIMENSIONS];
  pop_indexes(vm, indexes, ndim, sym->name);

  if (!sym->defined)
    dimension_numeric_auto(vm, sym, ndim, indexes);

  assert(sym->val.numarr != NULL);
  *numeric_element(vm, sym->val.numarr, sym->name, ndim, indexes) = val;
}

static void set_numeric(VM* vm, SYMBOL* sym, unsigned ndim, double val) {
  if (ndim == 0)
    set_numeric_simple(vm, sym, val);
  else
    set_numeric_element(vm, sym, ndim, val);
}

static void get_string_simple(VM* vm, SYMBOL* sym) {
  assert(sym != NULL && sym->kind == SYM_VARIABLE && sym->type == TYPE_STR);

  if (!sym->defined) {
    if (vm->strict_variables)
      run_error(vm, "Variable not found: %s\n", sym->name);
    sym->val.str = NULL;
    sym->defined = true;
  }

  push_str(vm, sym->val.str); // duplicates the string, uses "" if NULL
}

static void set_string_simple(VM* vm, SYMBOL* sym, char* val) {
  assert(sym != NULL && sym->kind == SYM_VARIABLE && sym->type == TYPE_STR);

  if (sym->val.str) {
    assert(sym->defined);
    efree(sym->val.str);
  }

  sym->val.str = val;
  sym->defined = true;
}

static void dimension_string(VM* vm, SYMBOL* sym, unsigned ndim, const unsigned max[]) {
  assert(sym != NULL && sym->kind == SYM_ARRAY && sym->type == TYPE_STR);

  sym->val.strarr = new_string_array(vm->array_base, ndim, max);
  if (sym->val.strarr == NULL) {
    sym->defined = false;
    run_error(vm, "invalid dimensions: %s\n", sym->name);
  }
  sym->defined = true;
}

static void dimension_string_auto(VM* vm, SYMBOL* sym, unsigned ndim, const unsigned indexes[]) {
  assert(sym != NULL && sym->val.strarr == NULL);

  unsigned max[MAX_DIMENSIONS];
  for (unsigned j = 0; j < ndim; j++)
    max[j] = indexes[j] < 10 ? 10 : indexes[j];

  dimension_string(vm, sym, ndim, max);
}

static char* * string_element(VM* vm, struct string_array * p, const char* name, unsigned ndim, const unsigned indexes[]) {
  char* * addr = NULL;
  if (!compute_string_element(p, ndim, indexes, &addr))
    run_error(vm, "array indexes invalid or out of range: %s\n", name);
  return addr;
}

static void get_string_element(VM* vm, SYMBOL* sym, unsigned ndim) {
  assert(sym != NULL && sym->kind == SYM_ARRAY && sym->type == TYPE_STR);

  unsigned indexes[MAX_DIMENSIONS];
  pop_indexes(vm, indexes, ndim, sym->name);

  if (!sym->defined)
    dimension_string_auto(vm, sym, ndim, indexes);

  assert(sym->val.strarr != NULL);
  push_str(vm, *string_element(vm, sym->val.strarr, sym->name, ndim, indexes));
}

static void set_string_element(VM* vm, SYMBOL* sym, unsigned ndim, char* val) {
  assert(sym != NULL && sym->kind == SYM_ARRAY && sym->type == TYPE_STR);

  unsigned indexes[MAX_DIMENSIONS];
  pop_indexes(vm, indexes, ndim, sym->name);

  if (!sym->defined)
    dimension_string_auto(vm, sym, ndim, indexes);

  assert(sym->val.strarr != NULL);
  char* * addr = string_element(vm, sym->val.strarr, sym->name, ndim, indexes);
  efree(*addr);
  *addr = val;
}

static void set_string(VM* vm, SYMBOL* sym, unsigned ndim, char* val) {
  if (ndim == 0)
    set_string_simple(vm, sym, val);
  else
    set_string_element(vm, sym, ndim, val);
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
  if (vm->stored_program.index == NULL ||
     !bcode_find_indexed_basic_line(vm->stored_program.index, basic_line, &bcode_line))
    run_error(vm, "Line not found: %u\n", basic_line);
  return bcode_line;
}

static void go_to_basic_line(VM* vm, unsigned basic_line) {
  vm->code_state.pc = find_basic_line(vm, basic_line);
  vm->code_state.code = &vm->stored_program;
  // vm->code_state.source_line will be set by the LINE command at that PC
}

// Obviously READ in a stored program reads the stored program's DATA.
// Immediate READ reads immediate DATA if any given, otherwise program DATA.
static const char* find_data(VM* vm) {
  const BCODE* bc = vm->stored_program.bcode;
  unsigned dp = vm->program_data;
  bool immediate = false;

  if (vm->code_state.code == &vm->immediate_code) {
    assert(vm->immediate_code.bcode != NULL);
    if (vm->immediate_code.bcode->has_data) {
      bc = vm->immediate_code.bcode;
      dp = vm->immediate_data;
      immediate = true;
    }
  }

  if (bc == NULL || !bc->has_data)
    run_error(vm, "no DATA\n");

  while (dp < bc->used && bc->inst[dp].op != B_DATA)
    dp++;

  if (dp >= bc->used)
    run_error(vm, "out of DATA\n");

  const char* s = bc->inst[dp++].u.str;
  if (s == NULL)
    run_error(vm, "internal error: null DATA\n");

  if (immediate)
    vm->immediate_data = dp;
  else
    vm->program_data = dp;

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

// Return index on FOR stack of given variable, or -1 if not there.
static int find_for(VM* vm, SYMID id) {
  for (int j = 0; j < (int)vm->for_sp; j++) {
    if (vm->for_stack[j].symbol_id == id)
      return j;
  }
  return -1;
}

// Move to next iteration of FOR loop at FOR stack index si.
// If the exit condition is met, remove the entry from the FOR stack.
// Otherwise update the iteration variable value and return to loop start.
static void next(VM* vm, int si) {
  struct for_loop * f = &vm->for_stack[si];
  SYMBOL* sym = symbol(vm->st, f->symbol_id);
  double x = sym->val.num + f->step;
  if (f->step > 0 && x > f->limit || f->step < 0 && x < f->limit) {
    // remove FOR stack index si
    vm->for_sp--;
    for (unsigned k = si; k < vm->for_sp; k++)
      vm->for_stack[k] = vm->for_stack[k + 1];
  }
  else {
    sym->val.num = x;
    vm->code_state = f->code_state;
  }
  if (vm->trace_for)
    dump_for_stack(vm, "final stack");
}

static void call_def(VM* vm, SYMBOL* sym, unsigned params) {
  assert(sym != NULL && sym->kind == SYM_DEF);

  if (!sym->defined)
    run_error(vm, "user-defined function has not been defined: %s\n", sym->name);
  struct def * def = sym->val.def;
  assert(def != NULL);

  if (vm->fn.code_state.code)
    run_error(vm, "nested user-defined function calls are not allowed\n");

  if (params != 1)
    run_error(vm, "unexpected number of parameters: %s: expected %u, received %u\n",
              sym->name, 1, params);

  if (vm->code_state.pc + params >= vm->code_state.code->bcode->used)
    run_error(vm, "program corrupt: missing parameters: %s\n", sym->name);

  vm->fn.code_state = vm->code_state;

  vm->def_code.bcode = def->bcode;
  vm->def_code.source = def->source;
  assert(vm->def_code.index == NULL);
  vm->code_state.code = &vm->def_code;
  vm->code_state.source_line = def->source_line;
  vm->code_state.pc = 0;

  BINST* param_inst = def->bcode->inst + 1; // the one and only parameter
  if (param_inst->op != B_PARAM) {
    print_binst(param_inst, params, NULL, vm->st, stderr);
    run_error(vm, "program corrupt: parameter expected\n");
  }
  SYMID param_id = param_inst->u.symbol_id;
  SYMBOL* param_sym = symbol(vm->st, param_id);
  assert(param_sym != NULL && param_sym->kind == SYM_VARIABLE && param_sym->type == TYPE_NUM);

  vm->fn.param_id = param_id;
  vm->fn.param_defined = param_sym->defined;
  vm->fn.param_val = param_sym->val.num;

  param_sym->val.num = pop(vm);
  param_sym->defined = true;

  vm->code_state.pc += params;
}

static void end_def(VM* vm) {
  if (vm->fn.code_state.code == NULL)
    run_error(vm, "unexpected END DEF\n");
  vm->code_state = vm->fn.code_state;
  vm->fn.code_state.code = NULL;
  SYMBOL* param_sym = symbol(vm->st, vm->fn.param_id);
  assert(param_sym != NULL);
  param_sym->defined = vm->fn.param_defined;
  param_sym->val.num = vm->fn.param_val;
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
      SYMBOL* sym = symbol(vm->st, vm->for_stack[j-1].symbol_id);
      printf("%s = %g, %g; ", sym->name, sym->val.num, vm->for_stack[j-1].limit);
    }
  }
  putc('\n', stdout);
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

  CuAssertPtrNotNull(tc, vm->st);

  CuAssertIntEquals(tc, false, vm->stopped);
  CuAssertIntEquals(tc, 0, vm->sp);
  CuAssertIntEquals(tc, 0, vm->ssp);
  CuAssertIntEquals(tc, 0, vm->rsp);
  CuAssertIntEquals(tc, 1, vm->col);
  CuAssertIntEquals(tc, 0, vm->for_sp);
  CuAssertIntEquals(tc, 0, vm->program_data);
  CuAssertIntEquals(tc, 0, vm->immediate_data);

  CuAssertTrue(tc, vm->fn.code_state.code == NULL);

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
