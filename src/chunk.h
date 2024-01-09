#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_CONSTANT_LONG,
  OP_RETURN,
} opcode_t;

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
  value_array_t constants;
  line_info_t lines;
} chunk_t;

void chunk_init(chunk_t *chunk);
void chunk_write(chunk_t *chunk, uint8_t byte, linenr_t line);
void chunk_free(chunk_t *chunk);

size_t chunk_add_constant(chunk_t *chunk, value_t value);
void chunk_write_constant(chunk_t *chunk, value_t constant, linenr_t line);

#endif
