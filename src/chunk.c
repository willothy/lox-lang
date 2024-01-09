#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void line_info_init(line_info_t *lines) {
	lines->count = 0;
	lines->capacity = 0;
	lines->lines = NULL;
}

void line_info_inc(line_info_t *lines, linenr_t line) {
	ASSERT(line >= lines->count); // lines should always be added in order
	if (lines->count == 0 || lines->lines[lines->count-1].line < line) {
		if (lines->capacity < lines->count + 1) {
			size_t old_capacity = lines->capacity;
			lines->capacity = GROW_CAPACITY(old_capacity);
			lines->lines = GROW_ARRAY(line_t, lines->lines, old_capacity, lines->capacity);
		}
		size_t start = 0;
		if (line > 0) {
			start = lines->lines[lines->count - 1].start + lines->lines[lines->count - 1].len;
		}
		lines->lines[lines->count] = (line_t) {
			.line = line,
			.start = start,
			.len = 0,
		};
		lines->count++;
	}
	lines->lines[lines->count-1].len++;
}

void line_info_free(line_info_t *lines) {
	FREE_ARRAY(line_t, lines->lines, lines->capacity);
	line_info_init(lines);
}

linenr_t line_info_get (const line_info_t *lines, size_t offset) {
	// ASSERT(offset < lines->lines[lines->count-1].start + lines->lines[lines->count-1].len);
	linenr_t linenr = 0;
	for (size_t i = 0; i < lines->count; i++) {
		line_t *line = &lines->lines[i];
		linenr = line->line;
		printf("offset %zu, line %zu, start %zu, len %zu\n", offset, line->line, line->start, line->len);
		if (offset >= line->start && offset < line->start + line->len) {
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

void chunk_write(Chunk *chunk, uint8_t byte, linenr_t line) {
	if (chunk->capacity < chunk->count + 1) {
		size_t old_capacity = chunk->capacity;
		chunk->capacity = GROW_CAPACITY(old_capacity);
		chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
	}

	// chunk->lines = GROW_ARRAY(line_t, chunk->lines, old_capacity, chunk->capacity);
	line_info_inc(&chunk->lines, line);

	chunk->code[chunk->count] = byte;
	// chunk->lines[chunk->count] = line;
	chunk->count++;
}

void chunk_free(Chunk *chunk) {
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	value_array_free(&chunk->constants);
	line_info_free(&chunk->lines);
	chunk_init(chunk);
}

size_t chunk_add_constant(Chunk *chunk, Value value) {
	value_array_write(&chunk->constants, value);
	return chunk->constants.count - 1;
}
