#include "common.h"
#include "debug.h"
#include <stdio.h>
#include "vm.h"

VM vm;

static void reset_stack() {
	vm.stack_top = vm.stack;
}

void vm_init() {
	reset_stack();
}

void vm_free() {
	// todo
}

void vm_push(value_t value) {
	*vm.stack_top = value;
	vm.stack_top++;
}

value_t vm_pop() {
	vm.stack_top--;
	return *vm.stack_top;
}

static interpret_result_t run() {
  #define READ_BYTE() (*vm.ip++)
  #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

	for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
		printf("  stack:  ");
		for (value_t* slot = vm.stack; slot < vm.stack_top; slot++) {
			printf("[ ");
			value_print(*slot);
			printf(" ]");
		}
		printf("\n");
		disassemble_instruction(vm.chunk, (size_t)(vm.ip - vm.chunk->code));
    #endif

		uint8_t instruction;
		switch (instruction = READ_BYTE()) {
		case OP_CONSTANT: {
			value_t constant = READ_CONSTANT();
			vm_push(constant);
			break;
		}
		case OP_CONSTANT_LONG: {
			// read 3 bytes to get operand index
			uint32_t index = READ_BYTE();
			index |= (uint16_t)READ_BYTE() << 8;
			index |= (uint16_t)READ_BYTE() << 16;
			value_t constant = vm.chunk->constants.values[index];
			vm_push(constant);
			break;
		}
		case OP_NEGATE: {
			vm_push(-vm_pop());
			break;
		}
		case OP_RETURN: {
			printf("return ");
			value_println(vm_pop());
			return INTERPRET_OK;
		}
		}
	}

  #undef READ_BYTE
  #undef READ_CONSTANT
}

interpret_result_t vm_interpret(chunk_t *chunk) {
	vm.chunk = chunk;
	vm.ip = vm.chunk->code;
	return run();
}
