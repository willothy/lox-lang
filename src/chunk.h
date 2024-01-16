#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// TODO: const / final vars

// TODO: use high bit for long flag instead of having separate opcodes
typedef enum {
  OP_CONSTANT,
  OP_CONSTANT_LONG,
  OP_DEFINE_GLOBAL,
  OP_DEFINE_GLOBAL_LONG,
  OP_GET_GLOBAL,
  OP_GET_GLOBAL_LONG,
  OP_SET_GLOBAL,
  OP_SET_GLOBAL_LONG,
  OP_GET_LOCAL,
  OP_GET_LOCAL_LONG,
  OP_SET_LOCAL,
  OP_SET_LOCAL_LONG,
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
  OP_CLOSE_UPVALUE,
  OP_LIST,
  OP_LIST_LONG,
  OP_DICT,
  OP_DICT_LONG,
  OP_GET_FIELD,
  OP_SET_FIELD,
  OP_COROUTINE,
  OP_YIELD,
  OP_AWAIT,
  OP_CALL,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_LOOP,
  OP_CLOSURE,
  OP_CLOSURE_LONG,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_EQUAL,
  // TODO: add more specific comparison operators for performance
  OP_GREATER,
  OP_LESS,
  OP_NOT,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  // OP_MODULO,
  OP_NEGATE,
  OP_RETURN,
  OP_POP,
} Opcode;

typedef size_t Linenr;

// Represents a line of instructions in the source text,
// for error reporting.
typedef struct {
  // The actual line number
  Linenr line;
  // The offset of the first instruction in the line
  size_t start;
  // The number of instructions in the line
  size_t len;
} Line;

typedef struct {
  // Info should always be added in sorted order, as this needs
  // to be fast.
  //
  // Lookup can be slower, as it is only used for error reporting.
  Line *lines;
  // This is *not* the number of lines in the source text,
  // but the number of lines that have at least one instruction.
  // Not guaranteed to start with line 1.
  size_t count;
  size_t capacity;
} LineInfo;

void line_info_init(LineInfo *lines);
void line_info_inc(LineInfo *lines, Linenr line);
void line_info_free(LineInfo *lines);
Linenr line_info_get(const LineInfo *lines, size_t offset);

// A bytecode chunk, containing a sequence of instructions,
// debug information, and the chunk's constants.
typedef struct {
  size_t count;
  size_t capacity;
  uint8_t *code;
  ValueArray constants;
  LineInfo lines;
} Chunk;

void chunk_init(Chunk *chunk);
void chunk_write(Chunk *chunk, uint8_t byte, Linenr line);
void chunk_free(Chunk *chunk);

uint32_t chunk_add_constant(Chunk *chunk, Value value);
uint32_t chunk_write_constant(Chunk *chunk, Value constant, Linenr line);
uint32_t chunk_last_instruction_len(Chunk *chunk);

#endif
