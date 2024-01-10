#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_INITIAL (FRAMES_MAX * UINT8_COUNT)

typedef struct {
  ObjectFunction *function;
  uint8_t *ip;
  Value *slots;
} CallFrame;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

typedef struct {
  // Call stack
  CallFrame frames[FRAMES_MAX];
  size_t frame_count;

  // Stack
  Value *stack_top;
  Value *stack;
  size_t stack_size;

  // Heap / globals
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

InterpretResult vm_interpret(const char *source);

#endif
