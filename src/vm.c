#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"
#include "compiler.h"
#include "object.h"
#include "value.h"

VM vm;

static void reset_stack() {
	vm.stack_top = vm.stack;
}

char *vm_init() {
	vm.stack = GROW_ARRAY(Value, vm.stack, 0, STACK_INITIAL);
	if (!vm.stack) {
		return "Could not allocate memory for stack";
	}
	vm.stack_size = STACK_INITIAL;
	reset_stack();
	vm.objects = NULL;
	table_init(&vm.strings);
	table_init(&vm.globals);
	return NULL;
}

void vm_free() {
	FREE_ARRAY(Value, vm.stack, vm.stack_size);
	table_free(&vm.strings);
	table_free(&vm.globals);
	free_objects();
}

void vm_push(Value value) {
	if (vm.stack_top - vm.stack == vm.stack_size) {
		size_t top = (size_t)vm.stack_top - (size_t)vm.stack;
		size_t old_size = vm.stack_size;
		size_t new_size = GROW_CAPACITY(old_size);

		#ifdef DEBUG_TRACE_EXECUTION
		printf("stack overflow at %zu, growing to %zu\n", old_size, new_size);
		#endif

		vm.stack = GROW_ARRAY(Value, vm.stack, old_size, new_size);
		vm.stack_size = new_size;
		// Update stack_top in case the location of the stack array changed due to realloc
		vm.stack_top = (Value*)((size_t)vm.stack + ((size_t)top) * sizeof(Value));
	}
	*vm.stack_top = value;
	vm.stack_top++;
}

Value vm_pop() {
	vm.stack_top--;
	return *vm.stack_top;
}

Value vm_peek(size_t distance) {
	return vm.stack_top[-1 - distance];
}

static void runtime_error(const char *format,...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	size_t inst = vm.ip - vm.chunk->code - 1;
	Linenr line = line_info_get(&vm.chunk->lines, inst);
	fprintf(stderr, "[line %zu] in script\n", line);
	reset_stack();
}

static void concatonate() {
	ObjectString *a = AS_STRING(vm_pop());
	ObjectString *b = AS_STRING(vm_pop());

	size_t length = a->length + b->length;
	char *chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	chars[length] = '\0';

	ObjectString *result = take_string(chars, length);
	vm_push(OBJ_VAL(result));
}

static InterpretResult run() {
  #define READ_BYTE() (*vm.ip++)
  #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
	#define READ_CONSTANT_LONG()    \
		(vm.chunk->constants.values[READ_BYTE() | (READ_BYTE() << 8) | (READ_BYTE() << 16)])
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
		for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
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
			Value constant = READ_CONSTANT();
			vm_push(constant);
			break;
		}
		case OP_CONSTANT_LONG: {
			Value constant = READ_CONSTANT_LONG();
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
			Value a = vm_pop();
			Value b = vm_pop();
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
			if (IS_STRING(vm_peek(0)) && IS_STRING(vm_peek(1))) {
				concatonate();
			} else if (IS_NUMBER(vm_peek(0)) && IS_NUMBER(vm_peek(1))) {
				double b = AS_NUMBER(vm_pop());
				double a = AS_NUMBER(vm_pop());
				vm_push(NUMBER_VAL(a + b));
			} else {
				runtime_error( "Operands must be two numbers or two strings.");
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
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
			return INTERPRET_OK;
		}
		case OP_POP: {
			vm_pop();
			break;
		}
		case OP_DEFINE_GLOBAL: {
			ObjectString *name = AS_STRING(READ_CONSTANT());
			table_set(&vm.globals, name, vm_peek(0));
			vm_pop();
			break;
		}
		case OP_DEFINE_GLOBAL_LONG: {
			ObjectString *name = AS_STRING(READ_CONSTANT_LONG());
			table_set(&vm.globals, name, vm_peek(0));
			vm_pop();
			break;
		}
		case OP_SET_GLOBAL: {
			ObjectString *name = AS_STRING(READ_CONSTANT());
			if (table_set(&vm.globals, name, vm_peek(0))) {
				table_delete(&vm.globals, name);
				// TODO: what should I do here?
				runtime_error("Undefined variable '%.*s'.", name->length, name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_SET_GLOBAL_LONG: {
			ObjectString *name = AS_STRING(READ_CONSTANT_LONG());
			if (table_set(&vm.globals, name, vm_peek(0))) {
				table_delete(&vm.globals, name);
				// TODO: same as above
				runtime_error("Undefined variable '%.*s'.", name->length, name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_GET_GLOBAL: {
			ObjectString *name = AS_STRING(READ_CONSTANT());
			Value value;
			if (!table_get(&vm.globals, name, &value)) {
				// This is the default behavior. I don't it, so my version of lox will
				// push nil onto the stack like Lua instead of throwing an error.
				//
				// runtime_error("Undefined variable '%.*s'.", name->length, name->chars);
				// return INTERPRET_RUNTIME_ERROR;

				// Just push nil instead :)
				value = NIL_VAL;
			}
			vm_push(value);
			break;
		}
		// TODO: figure out how to get rid of this duplication
		case OP_GET_GLOBAL_LONG: {
			ObjectString *name = AS_STRING(READ_CONSTANT_LONG());
			Value value;
			if (!table_get(&vm.globals, name, &value)) {
				// Same as above
				value = NIL_VAL;
			}
			vm_push(value);
			break;
		}
		case OP_GET_LOCAL: {
			uint8_t slot = READ_BYTE();
			vm_push(vm.stack[slot]);
			break;
		}
		case OP_GET_LOCAL_LONG: {
			uint32_t slot = READ_BYTE();
			slot |= READ_BYTE() << 8;
			slot |= READ_BYTE() << 16;
			vm_push(vm.stack[slot]);
			break;
		}
		case OP_SET_LOCAL: {
			uint8_t slot = READ_BYTE();
			vm.stack[slot] = vm_peek(0);
			break;
		}
		case OP_SET_LOCAL_LONG: {
			uint32_t slot = READ_BYTE();
			slot |= READ_BYTE() << 8;
			slot |= READ_BYTE() << 16;
			vm.stack[slot] = vm_peek(0);
			break;
		}
		case OP_PRINT: {
			value_println(vm_pop());
			break;
		}
		}
	}

  #undef READ_BYTE
  #undef READ_CONSTANT
	#undef READ_CONSTANT_LONG
	#undef BINARY_OP
}

InterpretResult vm_interpret(const char *src) {
	Chunk chunk;
	chunk_init(&chunk);

	if (!compile(src, &chunk)) {
		chunk_free(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	vm.chunk = &chunk;
	vm.ip = vm.chunk->code;

	InterpretResult res = run();

	vm.chunk = NULL;
	vm.ip = NULL;

	chunk_free(&chunk);

	return res;
}
