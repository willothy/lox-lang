#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
  chunk_t *chunk;
  uint8_t *ip;
  value_t stack[STACK_MAX];
  value_t *stack_top;
} VM;

void vm_init();
void vm_free();

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} interpret_result_t;

interpret_result_t vm_interpret(chunk_t *chunk);

#endif
