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
  linenr_t line;
  // The offset of the first instruction in tue line
  size_t start;
  size_t n_instructions;
} line_t;

typedef struct {
  size_t count;
  size_t capacity;
  uint8_t *code;
  ValueArray constants;
  // TODO: replace with line_t*, append to last line_t or create new line_t
  // when line changtes in chunk_write
  linenr_t *lines;
} Chunk;

void chunk_init(Chunk *chunk);
void chunk_write(Chunk *chunk, uint8_t byte, linenr_t line);
void chunk_free(Chunk *chunk);

size_t chunk_add_constant(Chunk *chunk, Value value);

#endif
