#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void line_info_init(LineInfo *lines) {
	lines->count = 0;
	lines->capacity = 0;
	lines->lines = NULL;
}

void line_info_inc(LineInfo *lines, Linenr line) {
	if (lines->count == 0 || lines->lines[lines->count-1].line < line) {
		if (lines->capacity < lines->count + 1) {
			size_t old_capacity = lines->capacity;
			lines->capacity = GROW_CAPACITY(old_capacity);
			lines->lines = GROW_ARRAY(Line, lines->lines, old_capacity, lines->capacity);
		}
		size_t start = 0;
		if (lines->count > 0) {
			start = lines->lines[lines->count - 1].start + lines->lines[lines->count - 1].len;
		}
		lines->lines[lines->count] = (Line) {
			.line = line,
			.start = start,
			.len = 0,
		};
		lines->count++;
	}
	lines->lines[lines->count-1].len++;
}

void line_info_free(LineInfo *lines) {
	FREE_ARRAY(Line, lines->lines, lines->capacity);
	line_info_init(lines);
}

Linenr line_info_get (const LineInfo *lines, size_t offset) {
	Linenr linenr = 0;
	for (size_t i = 0; i < lines->count; i++) {
		Line *line = &lines->lines[i];
		linenr = line->line;
		if (offset >= line->start && offset < line->start + line->len)  {
			break;
		}
	}
	return linenr;
}

void chunk_init(Chunk *chunk) {
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
	line_info_init(&chunk->lines);
	value_array_init(&chunk->constants);
}

void chunk_write(Chunk *chunk, uint8_t byte, Linenr line) {
	if (chunk->capacity < chunk->count + 1) {
		size_t old_capacity = chunk->capacity;
		chunk->capacity = GROW_CAPACITY(old_capacity);
		chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
	}

	line_info_inc(&chunk->lines, line);

	chunk->code[chunk->count] = byte;
	chunk->count++;
}

void chunk_free(Chunk *chunk) {
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	value_array_free(&chunk->constants);
	line_info_free(&chunk->lines);
	chunk_init(chunk);
}

uint32_t chunk_add_constant(Chunk *chunk, Value value) {
	value_array_write(&chunk->constants, value);
	return chunk->constants.count - 1;
}

uint32_t chunk_write_constant(Chunk *chunk, Value constant, Linenr line) {
	uint32_t index = chunk_add_constant(chunk, constant);

	if (index > UINT8_MAX) {
		chunk_write(chunk, OP_CONSTANT_LONG, line);
		// store operand as a 24-bit number, giving us more than 256 constant slots if needed
		chunk_write(chunk, (uint8_t)(index & 0xFF), line);
		chunk_write(chunk, (uint8_t)((index >> 8) & 0xFF), line);
		chunk_write(chunk, (uint8_t)((index >> 16) & 0xFF), line);
	} else {
		chunk_write(chunk, OP_CONSTANT, line);
		chunk_write(chunk, (uint8_t)index, line);
	}

	return index;
}
