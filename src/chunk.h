#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_RETURN,
} OpCode;

typedef size_t linenr_t;

// Represents a line of instructions in the source text,
// for error reporting.
typedef struct {
  // The actual line number
  linenr_t line;
  // The offset of the first instruction in the line
  size_t start;
  // The number of instructions in the line
  size_t len;
} line_t;

typedef struct {
  // Info should always be added in sorted order, as this needs
  // to be fast.
  //
  // Lookup can be slower, as it is only used for error reporting.
  line_t *lines;
  // This is *not* the number of lines in the source text,
  // but the number of lines that have at least one instruction.
  // Not guaranteed to start with line 1.
  size_t count;
  size_t capacity;
} line_info_t;

void line_info_init(line_info_t *lines);
void line_info_inc(line_info_t *lines, linenr_t line);
void line_info_free(line_info_t *lines);
linenr_t line_info_get(const line_info_t *lines, size_t offset);

// A bytecode chunk, containing a sequence of instructions,
// debug information, and the chunk's constants.
typedef struct {
  size_t count;
  size_t capacity;
  uint8_t *code;
  ValueArray constants;
  line_info_t lines;
} Chunk;

void chunk_init(Chunk *chunk);
void chunk_write(Chunk *chunk, uint8_t byte, linenr_t line);
void chunk_free(Chunk *chunk);

size_t chunk_add_constant(Chunk *chunk, Value value);

#endif
