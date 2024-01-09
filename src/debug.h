#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

void disassemble_chunk(chunk_t *chunk, const char *name);
size_t disassemble_instruction(chunk_t *chunk, size_t offset);

#endif
