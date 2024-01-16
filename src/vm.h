#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

typedef struct {
  // The active coroutine.
  Coroutine *running;
  // The toplevel coroutine. This should always be the first
  // coroutine in the GC linked list, and should always be accessible
  // from a coroutine object by traversing its ancestors.
  Coroutine *main;

  // GC
  size_t gray_count;
  size_t gray_capacity;
  Object **gray_stack;
  // TODO: add nursery and tenured spaces for generational GC
  size_t bytes_allocated;
  size_t next_gc;

  bool mark_value;

  // Heap / globals
  Upvalue *open_upvalues;
  Object *objects;
  Table strings;
  // TODO: come up with a faster way to look up globals (maybe by index instead
  // of hash?)
  Table globals;
} VM;

extern VM vm;

char *vm_init();
void vm_free();

void vm_push(Value value);
Value vm_pop();
Value vm_peek(size_t distance);
bool vm_call(Closure *closure, uint8_t argc);
InterpretResult vm_run(bool repl);

InterpretResult vm_interpret(Function *function);

#endif
