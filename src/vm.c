#include <stdarg.h>
#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"
#include "compiler.h"
#include "value.h"

VM vm;

static void reset_stack() {
	vm.stack_top = vm.stack;
}

char *vm_init() {
	vm.stack = GROW_ARRAY(value_t, vm.stack, 0, STACK_INITIAL);
	if (!vm.stack) {
		return "Could not allocate memory for stack";
	}
	vm.stack_size = STACK_INITIAL;
	reset_stack();
	return NULL;
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

value_t vm_peek(size_t distance) {
	return vm.stack_top[-1 - distance];
}

static void runtime_error(const char *format,...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	size_t inst = vm.ip - vm.chunk->code - 1;
	linenr_t line = line_info_get(&vm.chunk->lines, inst);
	fprintf(stderr, "[line %zu] in script\n", line);
	reset_stack();
}

static interpret_result_t run() {
  #define READ_BYTE() (*vm.ip++)
  #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
	#define BINARY_OP(value_type, op) \
		if (!IS_NUMBER(vm_peek(0)) || !IS_NUMBER(vm_peek(1))) { \
			runtime_error("Operands must be numbers."); \
			return INTERPRET_RUNTIME_ERROR; \
		} \
		do { \
			double b = AS_NUMBER(vm_pop()); \
			double a = AS_NUMBER(vm_pop()); \
			vm_push(value_type(a op b)); \
		} while (false)

	#ifdef DEBUG_TRACE_EXECUTION
	printf("== trace ==\n");
	#endif

	for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
		printf("stack:  ");
		for (value_t* slot = vm.stack; slot < vm.stack_top; slot++) {
			printf("[ ");
			value_print(*slot);
			printf(" ]");
		}
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
		case OP_NIL: {
			vm_push(NIL_VAL);
			break;
		}
		case OP_TRUE: {
			vm_push(BOOL_VAL(true));
			break;
		}
		case OP_FALSE: {
			vm_push(BOOL_VAL(false));
			break;
		}
		case OP_NOT: {
			vm_push(BOOL_VAL(IS_FALSY(vm_pop())));
			break;
		}
		case OP_EQUAL: {
			value_t a = vm_pop();
			value_t b = vm_pop();
			vm_push(BOOL_VAL(value_equal(a, b)));
			break;
		}
		case OP_GREATER: {
			BINARY_OP(BOOL_VAL, >);
			break;
		}
		case OP_LESS: {
			BINARY_OP(BOOL_VAL, <);
			break;
		}
		case OP_ADD: {
			BINARY_OP(NUMBER_VAL, +);
			break;
		}
		case OP_SUBTRACT: {
			BINARY_OP(NUMBER_VAL, -);
			break;
		}
		case OP_MULTIPLY: {
			BINARY_OP(NUMBER_VAL, *);
			break;
		}
		case OP_DIVIDE: {
			BINARY_OP(NUMBER_VAL, /);
			break;
		}
		// case OP_MODULO: {
		// 	BINARY_OP(%);
		// 	break;
		// }
		case OP_NEGATE: {
			if (!IS_NUMBER(vm_peek(0))) {
				runtime_error("Operand must be a number");
				return INTERPRET_RUNTIME_ERROR;
			}
			vm_push(NUMBER_VAL(-AS_NUMBER(vm_pop())));
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

interpret_result_t vm_interpret(const char *src) {
	chunk_t chunk;
	chunk_init(&chunk);

	if (!compile(src, &chunk)) {
		chunk_free(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	vm.chunk = &chunk;
	vm.ip = vm.chunk->code;

	interpret_result_t res = run();

	vm.chunk = NULL;
	vm.ip = NULL;

	chunk_free(&chunk);

	return res;
}
