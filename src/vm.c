#include "common.h"
#include "debug.h"
#include "vm.h"

VM vm;

void vm_init() {
	// todo
}

void vm_free() {
	// todo
}

static interpret_result_t run() {
  #define READ_BYTE() (*vm.ip++)
  #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

	for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
		disassemble_instruction(vm.chunk, (size_t)(vm.ip - vm.chunk->code));
    #endif
		uint8_t instruction;
		switch (instruction = READ_BYTE()) {
		case OP_CONSTANT: {
			value_t constant = READ_CONSTANT();
			value_print(constant);
			printf("\n");
			break;
		}
		case OP_RETURN: {
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
