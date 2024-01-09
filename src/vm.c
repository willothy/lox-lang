#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

VM vm;

static void reset_stack() {
	vm.stack_top = vm.stack;
}

void vm_init() {
	vm.stack = GROW_ARRAY(value_t, vm.stack, 0, STACK_INITIAL);
	vm.stack_size = STACK_INITIAL;
	reset_stack();
}

void vm_free() {
	FREE_ARRAY(value_t, vm.stack, vm.stack_size);
}

void vm_push(value_t value) {
	if (vm.stack_top - vm.stack == vm.stack_size) {
		size_t top = (size_t)vm.stack_top - (size_t)vm.stack;
		size_t old_size = vm.stack_size;
		size_t new_size = GROW_CAPACITY(old_size);

		#ifdef DEBUG_TRACE_EXECUTION
		printf("stack overflow at %zu, growing to %zu\n", old_size, new_size);
		#endif

		vm.stack = GROW_ARRAY(value_t, vm.stack, old_size, new_size);
		vm.stack_size = new_size;
		// Update stack_top in case the location of the stack array changed due to realloc
		vm.stack_top = (value_t*)((size_t)vm.stack + ((size_t)top) * sizeof(value_t));
	}
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
	#define BINARY_OP(op) \
		do { \
			double b = vm_pop(); \
			double a = vm_pop(); \
			vm_push(a op b); \
		} while (false)

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
		case OP_ADD: {
			BINARY_OP(+);
			break;
		}
		case OP_SUBTRACT: {
			BINARY_OP(-);
			break;
		}
		case OP_MULTIPLY: {
			BINARY_OP(*);
			break;
		}
		case OP_DIVIDE: {
			BINARY_OP(/);
			break;
		}
		// case OP_MODULO: {
		// 	BINARY_OP(%);
		// 	break;
		// }
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
	#undef BINARY_OP
}

interpret_result_t vm_interpret(chunk_t *chunk) {
	vm.chunk = chunk;
	vm.ip = vm.chunk->code;
	return run();
}
