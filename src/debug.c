#include <stdio.h>

#include "debug.h"
#include "value.h"

static size_t simple_instruction(const char *name, size_t offset) {
	printf("%s\n", name);
	return offset + 1;
}

static size_t constant_instruction(const char* name, chunk_t *chunk, int offset) {
	uint8_t idx = chunk->code[offset  + 1];
	printf("%-16s %4d '", name, idx);
	value_print(chunk->constants.values[idx]);
	printf("'\n");
	return offset + 2;
}

static size_t constant_long_instruction(const char* name, chunk_t *chunk, int offset) {
	uint32_t idx = chunk->code[offset  + 1];
	idx |= (uint16_t)chunk->code[offset + 2] << 8;
	idx |= (uint16_t)chunk->code[offset + 3] << 16;
	printf("%-16s %4d '", name, idx);
	value_print(chunk->constants.values[idx]);
	printf("'\n");
	return offset + 4;
}

void disassemble_chunk(chunk_t *chunk, const char *name) {
	printf("== %s ==\n", name);

	for (size_t offset = 0; offset < chunk->count;) {
		offset = disassemble_instruction(chunk, offset);
	}
}

size_t disassemble_instruction(chunk_t *chunk, size_t offset) {
	printf("%04zu ", offset);

	linenr_t linenr = line_info_get(&chunk->lines, offset);
	if (offset > 0 && linenr == line_info_get(&chunk->lines, offset - 1)) {
		for (int pad=0; pad < linenr % 10; pad++) {
			printf(" ");
		}
		printf("| ");
	} else {
		printf("%4zu ", linenr);
	}

	uint8_t instruction = chunk->code[offset];
	switch (instruction) {
	case OP_RETURN:
		return simple_instruction("OP_RETURN", offset);
	case OP_CONSTANT:
		return constant_instruction("OP_CONSTANT", chunk, offset);
	case OP_CONSTANT_LONG:
		return constant_long_instruction("OP_CONSTANT_LONG", chunk, offset);
	case OP_ADD:
		return simple_instruction("OP_ADD", offset);
	case OP_SUBTRACT:
		return simple_instruction("OP_SUBTRACT", offset);
	case OP_MULTIPLY:
		return simple_instruction("OP_MULTIPLY", offset);
	case OP_DIVIDE:
		return simple_instruction("OP_DIVIDE", offset);
	case OP_NEGATE:
		return simple_instruction("OP_NEGATE", offset);
	default:
		printf("Unknown opcode %d\n", instruction);
		return offset + 1;
	}
}
