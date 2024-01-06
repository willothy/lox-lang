#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_RETURN,
} OpCode;

typedef size_t linenr_t;

typedef struct {
  size_t count;
  size_t capacity;
  uint8_t *code;
  ValueArray constants;
  linenr_t *lines;
} Chunk;

void chunk_init(Chunk *chunk);
void chunk_write(Chunk *chunk, uint8_t byte, linenr_t line);
void chunk_free(Chunk *chunk);

size_t chunk_add_constant(Chunk *chunk, Value value);

#endif
