#include <stdio.h>

#include "debug.h"
#include "chunk.h"
#include "object.h"
#include "value.h"

static size_t simple_instruction(const char *name, size_t offset) {
	printf("%s\n", name);
	return offset + 1;
}

static size_t constant_instruction(const char* name, Chunk *chunk, int offset) {
	uint8_t idx = chunk->code[offset + 1];
	printf("%-16s %4d '", name, idx);
	value_print(chunk->constants.values[idx]);
	printf("'\n");
	return offset + 2;
}

static size_t constant_long_instruction(const char* name, Chunk *chunk, int offset) {
	uint32_t idx = chunk->code[offset  + 1];
	idx |= (uint16_t)chunk->code[offset + 2] << 8;
	idx |= (uint16_t)chunk->code[offset + 3] << 16;
	printf("%-16s %4d '", name, idx);
	// value_print(chunk->constants.values[idx]);
	printf("'\n");
	return offset + 4;
}

static size_t byte_instruction(const char* name, Chunk *chunk, int offset) {
	uint8_t idx = chunk->code[offset + 1];
	printf("%-16s %4d\n", name, idx);
	return offset + 2;
}

static size_t byte_long_instruction(const char* name, Chunk *chunk, int offset) {
	uint32_t idx = chunk->code[offset  + 1];
	idx |= (uint16_t)chunk->code[offset + 2] << 8;
	idx |= (uint16_t)chunk->code[offset + 3] << 16;
	printf("%-16s %4d\n", name, idx);
	return offset + 4;
}

static size_t jump_instruction(const char* name, int sign, Chunk *chunk, int offset) {
	uint32_t jump = (uint32_t)(chunk->code[offset + 1] << 24);
	jump |= (uint32_t)(chunk->code[offset + 2] << 16);
	jump |= (uint32_t)(chunk->code[offset + 3] << 8);
	jump |= (uint32_t)(chunk->code[offset + 4]);

	printf("%-16s %4d -> %d\n", name, offset, offset + 5 + sign * jump);

	return offset + 5;
}

void disassemble_chunk(Chunk *chunk, const char *name) {
	printf("== %s ==\n", name);

	for (size_t offset = 0; offset < chunk->count;) {
		offset = disassemble_instruction(chunk, offset);
	}
}

uint8_t instruction_length(Chunk *chunk, size_t offset) {
	uint8_t instruction = chunk->code[offset];
	switch (instruction) {
	case OP_CLOSURE: {
		offset++;
		uint8_t constant = chunk->code[offset++];

		uint8_t rv = 2;
		Function *function = AS_FUNCTION(chunk->constants.values[constant]);
		for (int i = 0; i < function->upvalue_count; i++) {
			rv += 2;
		}

		return rv;
	}
	case OP_CLOSURE_LONG: {
		offset++;
		uint32_t constant = chunk->code[offset++];
		constant |= chunk->code[offset++] << 8;
		constant |= chunk->code[offset++] << 16;

		uint8_t rv = 4;
		Function *function = AS_FUNCTION(chunk->constants.values[constant]);
		for (int i = 0; i < function->upvalue_count; i++) {
			rv += 2;
		}

		return rv;
	}
	case OP_DICT:
	case OP_CALL:
	case OP_LIST:
	case OP_GET_UPVALUE:
	case OP_SET_UPVALUE:
	case OP_CONSTANT:
	case OP_DEFINE_GLOBAL:
	case OP_GET_GLOBAL:
	case OP_SET_GLOBAL:
	case OP_GET_LOCAL:
	case OP_SET_LOCAL:
		return 2;
	case OP_DICT_LONG:
	case OP_LIST_LONG:
	case OP_CONSTANT_LONG:
	case OP_DEFINE_GLOBAL_LONG:
	case OP_GET_GLOBAL_LONG:
	case OP_SET_GLOBAL_LONG:
	case OP_GET_LOCAL_LONG:
	case OP_SET_LOCAL_LONG:
		return 4;
	case OP_JUMP:
	case OP_JUMP_IF_FALSE:
	case OP_LOOP:
		return 5;
	case OP_COROUTINE:
	case OP_YIELD:
	case OP_AWAIT:
	case OP_NIL:
	case OP_TRUE:
	case OP_FALSE:
	case OP_NOT:
	case OP_EQUAL:
	case OP_GREATER:
	case OP_LESS:
	case OP_ADD:
	case OP_SUBTRACT:
	case OP_MULTIPLY:
	case OP_DIVIDE:
	case OP_NEGATE:
	case OP_SET_FIELD:
	case OP_GET_FIELD:
	case OP_RETURN:
	case OP_CLOSE_UPVALUE:
	case OP_POP:
		return 1;
	default:
		return 1;
	}
}

size_t disassemble_instruction(Chunk *chunk, size_t offset) {
	printf("%04zu ", offset);

	Linenr linenr = line_info_get(&chunk->lines, offset);
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
	case OP_DICT:
		return byte_instruction("OP_DICT", chunk, offset);
	case OP_DICT_LONG:
		return byte_long_instruction("OP_DICT_LONG", chunk, offset);
	case OP_SET_FIELD:
		return simple_instruction("OP_SET_FIELD", offset);
	case OP_GET_FIELD:
		return simple_instruction("OP_GET_FIELD", offset);
	case OP_RETURN:
		return simple_instruction("OP_RETURN", offset);
	case OP_CALL:
		return byte_instruction("OP_CALL", chunk, offset);
	case OP_LIST:
		return byte_instruction("OP_LIST", chunk, offset);
	case OP_LIST_LONG:
		return byte_long_instruction("OP_LIST_LONG", chunk, offset);
	case OP_CLOSURE: {
		offset++;
		uint8_t constant = chunk->code[offset++];
		printf("%-16s %4d ", "OP_CLOSURE", constant);
		value_println(chunk->constants.values[constant]);

		Function *function = AS_FUNCTION(chunk->constants.values[constant]);
		for (int i = 0; i < function->upvalue_count; i++) {
			uint8_t is_local = chunk->code[offset++];
			uint8_t index = chunk->code[offset++];
			printf("%04zu      |                     %s %d\n", offset - 2, is_local ? "local" : "upvalue", index);
		}

		return offset;
	}
	case OP_CLOSURE_LONG: {
		offset++;
		uint32_t constant = chunk->code[offset++];
		constant |= chunk->code[offset++] << 8;
		constant |= chunk->code[offset++] << 16;
		printf("%-16s %4d ", "OP_CLOSURE_LONG", constant);
		value_println(chunk->constants.values[constant]);

		Function *function = AS_FUNCTION(chunk->constants.values[constant]);
		for (int i = 0; i < function->upvalue_count; i++) {
			uint8_t is_local = chunk->code[offset++];
			uint8_t index = chunk->code[offset++];
			printf("%04zu      |                     %s %d\n", offset - 2, is_local ? "local" : "upvalue", index);
		}

		return offset;
	}
	case OP_GET_UPVALUE:
		return byte_instruction("OP_GET_UPVALUE", chunk, offset);
	case OP_SET_UPVALUE:
		return byte_instruction("OP_SET_UPVALUE", chunk, offset);
	case OP_CLOSE_UPVALUE:
		return simple_instruction("OP_CLOSE_UPVALUE", offset);
	case OP_POP:
		return simple_instruction("OP_POP", offset);
	case OP_CONSTANT:
		return constant_instruction("OP_CONSTANT", chunk, offset);
	case OP_CONSTANT_LONG:
		return constant_long_instruction("OP_CONSTANT_LONG", chunk, offset);
	case OP_DEFINE_GLOBAL:
		return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
	case OP_DEFINE_GLOBAL_LONG:
		return constant_long_instruction("OP_DEFINE_GLOBAL_LONG", chunk, offset);
	case OP_GET_GLOBAL:
		return constant_instruction("OP_GET_GLOBAL", chunk, offset);
	case OP_GET_GLOBAL_LONG:
		return constant_long_instruction("OP_GET_GLOBAL_LONG", chunk, offset);
	case OP_SET_GLOBAL:
		return constant_instruction("OP_SET_GLOBAL", chunk, offset);
	case OP_SET_GLOBAL_LONG:
		return constant_long_instruction("OP_SET_GLOBAL_LONG", chunk, offset);
	case OP_GET_LOCAL:
		return byte_instruction("OP_GET_LOCAL", chunk, offset);
	case OP_GET_LOCAL_LONG:
		return byte_long_instruction("OP_GET_LOCAL_LONG", chunk, offset);
	case OP_SET_LOCAL:
		return byte_instruction("OP_SET_LOCAL", chunk, offset);
	case OP_SET_LOCAL_LONG:
		return byte_long_instruction("OP_SET_LOCAL_LONG", chunk, offset);
	case OP_JUMP:
		return jump_instruction("OP_JUMP", 1, chunk, offset);
	case OP_JUMP_IF_FALSE:
		return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
	case OP_LOOP:
		return jump_instruction("OP_LOOP", -1, chunk, offset);
	case OP_COROUTINE:
		return simple_instruction("OP_COROUTINE", offset);
	case OP_YIELD:
		return simple_instruction("OP_YIELD", offset);
	case OP_AWAIT:
		return simple_instruction("OP_AWAIT", offset);
	case OP_NIL:
		return simple_instruction("OP_NIL", offset);
	case OP_TRUE:
		return simple_instruction("OP_TRUE", offset);
	case OP_FALSE:
		return simple_instruction("OP_FALSE", offset);
	case OP_NOT:
		return simple_instruction("OP_NOT", offset);
	case OP_EQUAL:
		return simple_instruction("OP_EQUAL", offset);
	case OP_GREATER:
		return simple_instruction("OP_GREATER", offset);
	case OP_LESS:
		return simple_instruction("OP_LESS", offset);
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
