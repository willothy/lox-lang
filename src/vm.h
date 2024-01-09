#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"

#define STACK_INITIAL 256

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} interpret_result_t;

typedef struct {
  chunk_t *chunk;
  uint8_t *ip;
  value_t *stack_top;
  value_t *stack;
  size_t stack_size;
} VM;

void vm_init();
void vm_free();

void vm_push(value_t value);
value_t vm_pop();

interpret_result_t vm_interpret(chunk_t *chunk);

#endif
