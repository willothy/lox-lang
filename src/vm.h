#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define STACK_INITIAL 256

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

typedef struct {
  Chunk *chunk;
  uint8_t *ip;
  Value *stack_top;
  Value *stack;
  size_t stack_size;
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
