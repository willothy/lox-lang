#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "memory.h"
#include "vm.h"
#include "object.h"
#include "value.h"

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

VM vm;

static Value clock_native(uint8_t argc, Value *args) {
	return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

// The print() function, replacing the print statement from the book.
static Value print_native(uint8_t argc, Value *args) {
#ifdef DEBUG_TRACE_EXECUTION
	// Make prints easier to see when there's a lot of tracing output.
	printf("-----OUTPUT-----\n");
#endif

	value_println(args[0]);

#ifdef DEBUG_TRACE_EXECUTION
	printf("----------------\n");
#endif
	return NIL_VAL;
}

static Value coro_reset_native(uint8_t argc, Value *args) {
	if (!IS_COROUTINE(args[0])) {
		return NIL_VAL;
	}
	Coroutine *co = AS_COROUTINE(args[0]);
	coroutine_reset(co);
	return NIL_VAL;
}

static Value type_native(uint8_t argc, Value *args) {
	const ConstStr type = value_type_name(args[0]);
	String *string = copy_string(type.chars, type.length);

	return OBJ_VAL(string);
}

static Value is_type_native(uint8_t argc, Value *args) {
	const Value value = args[0];
	const Value expected = args[1];

#ifdef DYNAMIC_TYPE_CHECKING
	if (!IS_STRING(expected)) {
		// TODO: this should be an error
		return BOOL_VAL(false);
	}
#endif

	const String *string = AS_STRING(expected);
	// "nil" is the shortest type name
	if (string->length < 3) {
		return BOOL_VAL(false);
	}

	switch (string->chars[0]) {
	case 'n':
		if (string->chars[1] == 'i' && string->chars[2] == 'l') {
			return BOOL_VAL(IS_NIL(value));
		} else if (string->chars[1] == 'u') {
			if (string->length != 6 || strncmp(&string->chars[2], "mber", 4) != 0) {
				break;
			}
			return BOOL_VAL(IS_NUMBER(value));
		} else if (string->chars[1] == 'a') {
			if (string->length != 6 || strncmp(&string->chars[2], "tive", 4) != 0) {
				break;
			}
			return BOOL_VAL(IS_NATIVE(value));
		}
		break;
	case 'b':
		if (string->length != 4 || strncmp(&string->chars[1], "ool", 3) != 0) {
			break;
		}
		return BOOL_VAL(IS_BOOL(value));
	case 's':
		if (string->length != 6 || strncmp(&string->chars[1], "tring", 5) != 0) {
			break;
		}
		return BOOL_VAL(IS_STRING(value));
	case 'o':
		if (string->length != 6 || strncmp(&string->chars[1], "bject", 5) != 0) {
			break;
		}
		return BOOL_VAL(IS_OBJ(value));
	case 'f':
		if (string->length != 8 || strncmp(&string->chars[1], "unction", 7) != 0) {
			break;
		}
		// do i need to check closure here too?
		return BOOL_VAL(IS_FUNCTION(value));
	default:
		break;
	}

	// TODO: this should be an error
	return FALSE_VAL;
}

static void vm_reset() {
	vm.running = vm.main;
	coroutine_reset(vm.main);
}

static void define_native(const char *name, NativeFnPtr function, uint8_t arity) {
	Coroutine *co = vm.running;
	vm_push(OBJ_VAL(copy_string(name, strlen(name))));
	vm_push(OBJ_VAL(native_new(function, arity)));
	table_set(&vm.globals, AS_STRING(vm.running->stack[0]), vm.running->stack[1]);
	vm_pop();
	vm_pop();
}

char *vm_init() {
	vm.objects = NULL;
	vm.open_upvalues = NULL;

	vm.bytes_allocated = 0;
	vm.next_gc = 1024 * 1024;

	vm.mark_value = true;

	vm.gray_count = 0;
	vm.gray_capacity = 0;
	vm.gray_stack = NULL;

	vm.main = NULL;
	vm.running = NULL;
	vm.main = coroutine_new(NULL);
	vm.running = vm.main;

	table_init(&vm.strings);
	table_init(&vm.globals);

	define_native("clock", clock_native, 0);
	define_native("print", print_native, 1);
	define_native("type", type_native, 1);
	define_native("is", is_type_native, 2);
	define_native("reset", coro_reset_native, 1);

	return NULL;
}

void vm_free() {
	table_free(&vm.globals);
	table_free(&vm.strings);
	free_objects();
}

void vm_push(Value value) {
	if (!vm.running) {
		return;
	}
	coroutine_push(vm.running, value);
}

Value vm_pop() {
	if (!vm.running) {
		return NIL_VAL;
	}
	return coroutine_pop(vm.running);
}

Value vm_peek(size_t distance) {
	return coroutine_peek(vm.running, distance);
}

static void runtime_error(const char *format,...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	// TODO: stack trace for coroutines

	// Print the stack trace
	// for (size_t i = 0; i < vm.frame_count; i++) {
	// 	CallFrame *frame = &vm.frames[i];
	// 	Function *function = frame->closure->function;
	// 	size_t inst = frame->ip - function->chunk.code - 1;
	// 	Linenr line = line_info_get(&function->chunk.lines, inst);
	// 	fprintf(stderr, "[line %zu] in ", line);
	// 	if (function->name == NULL) {
	// 		fprintf(stderr, "script\n");
	// 	} else {
	// 		fprintf(stderr, "%s()\n", function->name->chars);
	// 	}
	// }

	vm_reset();
}

static void runtime_type_error(Value value, const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	const ConstStr type = value_type_name(value);
	printf("%.*s\n", (int)type.length, type.chars);
	// value_println(value);

	// TODO: stack trace for coroutines

	// Print the stack trace
	// for (size_t i = 0; i < vm.frame_count; i++) {
	// 	CallFrame *frame = &vm.frames[i];
	// 	Function *function = frame->closure->function;
	// 	size_t inst = frame->ip - function->chunk.code - 1;
	// 	Linenr line = line_info_get(&function->chunk.lines, inst);
	// 	fprintf(stderr, "[line %zu] in ", line);
	// 	if (function->name == NULL) {
	// 		fprintf(stderr, "script\n");
	// 	} else {
	// 		fprintf(stderr, "%s()\n", function->name->chars);
	// 	}
	// }

	vm_reset();
}

static void concatonate() {
	String *a = AS_STRING(vm_peek(0));
	String *b = AS_STRING(vm_peek(1));

	size_t length = a->length + b->length;
	char *chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	chars[length] = '\0';

	String *result = take_string(chars, length);

	vm_pop();
	vm_pop();

	vm_push(OBJ_VAL(result));
}

static Upvalue* upvalue_capture(Value *local) {
	Upvalue *prev_upvalue = NULL;

	Upvalue *upvalue = vm.open_upvalues;
	while (upvalue != NULL && upvalue->location > local) {
		prev_upvalue = upvalue;
		upvalue = upvalue->next;
	}

	if (upvalue != NULL && upvalue->location == local) {
		return upvalue;
	}

	Upvalue *created = upvalue_new(local);
	created->next = upvalue;

	if (prev_upvalue == NULL) {
		vm.open_upvalues = created;
	} else {
		prev_upvalue->next = created;
	}

	return created;
}

static void close_upvalues(Value *last) {
	while (vm.open_upvalues != NULL && vm.open_upvalues->location >= last) {
		Upvalue *upvalue = vm.open_upvalues;
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		vm.open_upvalues = upvalue->next;
	}
}

static bool set_field(Value container, Value key, Value value) {
	if (IS_LIST(container)) {
#ifdef DYNAMIC_TYPE_CHECKING
		if (!IS_NUMBER(key)) {
			runtime_error("List indices must be integers.");
			return false;
		}
		// ensure number is an integer
		if (AS_NUMBER(key) != (size_t)AS_NUMBER(key)) {
			runtime_error("List indices must be integral.");
			return false;
		}
#endif

		list_set(AS_LIST(container), AS_NUMBER(key), value);
		return true;
	} else if (IS_DICT(container)) {
#ifdef DYNAMIC_TYPE_CHECKING
		if (!IS_STRING(key)) {
			runtime_error("Dictionary keys must be strings.");
			return false;
		}
#endif
		dict_set(AS_DICT(container), AS_STRING(key), value);
		return true;
	}
	ConstStr type = value_type_name(container);
	runtime_error("Attempted to mutably index a %.*s value.", type.length, type.chars);
	return false;
}

static bool get_field(Value container, Value key) {
	if (IS_LIST(container)) {
#ifdef DYNAMIC_TYPE_CHECKING
		if (!IS_NUMBER(key)) {
			runtime_error("List indices must be integers.");
			return false;
		}
		// ensure number is an integer
		if (AS_NUMBER(key) != (size_t)AS_NUMBER(key)) {
			runtime_error("List indices must be integral.");
			return false;
		}
#endif

		vm_push(list_get(AS_LIST(container), AS_NUMBER(key)));
		return true;
	} else if (IS_DICT(container)) {
#ifdef DYNAMIC_TYPE_CHECKING
		if (!IS_STRING(key)) {
			runtime_error("Dictionary keys must be strings.");
			return false;
		}
#endif

		vm_push(dict_get(AS_DICT(container), AS_STRING(key)));
		return true;
	}
	ConstStr type = value_type_name(container);
	runtime_error("Attempted to index a %.*s value.", type.length, type.chars);
	return false;
}

static bool call(Closure *closure, uint8_t argc) {
#ifdef DYNAMIC_TYPE_CHECKING
	if (argc != closure->function->arity) {
		runtime_error("Expected %d arguments but got %d.", closure->function->arity, argc);
		return false;
	}
#endif
	Coroutine *co = vm.running;

	if (co->frame_count == FRAMES_MAX) {
		runtime_error("Stack overflow.");
		return false;
	} else if (co->frame_count + 1 >= co->frame_capacity) {
		size_t new_capacity = GROW_CAPACITY(co->frame_capacity);
		if (new_capacity > FRAMES_MAX) {
			new_capacity = FRAMES_MAX;
		}
		co->frames = GROW_ARRAY(CallFrame, co->frames, co->frame_capacity, new_capacity);
		co->frame_capacity = new_capacity;
	}

	CallFrame *frame = &co->frames[co->frame_count++];
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;
	frame->slots = co->stack_top - argc - 1; // -1 to account for reserved stack slot 0

	co->current_frame = frame;

	return true;
}

static bool call_native(NativeFunction *native, uint8_t argc) {
#ifdef NATIVE_ARITY_CHECKING
	if (argc != native->arity) {
		runtime_error("Expected %d arguments but got %d.", native->arity, argc);
		return false;
	}
#endif

	Value result = native->function(argc, vm.running->stack_top - argc);
	vm.running->stack_top -= argc + 1;
	vm_push(result);
	return true;
}

static bool call_coroutine(Coroutine *co, uint8_t argc) {
	switch(co->state){
	case COROUTINE_RUNNING:
		runtime_error("Attempted to resume a running coroutine.");
		return false;
	case COROUTINE_COMPLETE:
		runtime_error("Attempted to resume a finished coroutine.");
		return false;
	case COROUTINE_ERROR:
		runtime_error("Attempted to resume a dead (errored) coroutine.");
		return false;
	case COROUTINE_READY:
		break;
	}

	coroutine_push(co, OBJ_VAL(co));

	for (size_t i = 0; i < argc; i++) {
		coroutine_push(co, vm_pop());
	}

	co->parent = vm.running;
	vm.running = co;

	return true;
}

static bool call_value(Value callee, uint8_t argc) {
	if (IS_OBJ(callee)) {
		switch (OBJ_TYPE(callee)) {
		case OBJ_CLOSURE:
			return call(AS_CLOSURE(callee), argc);
		case OBJ_NATIVE:
			return call_native(AS_NATIVE(callee), argc);
		case OBJ_COROUTINE:
			return call_coroutine(AS_COROUTINE(callee), argc);
		case OBJ_FUNCTION: // should be unreachable (for now)
			// return call(AS_FUNCTION(callee), argc);
			break;
		case OBJ_LIST:
		case OBJ_UPVALUE:
		case OBJ_DICT:
		case OBJ_STRING:
			break;
		}
	}
	runtime_type_error(callee, "Can only call functions and classes, attempted to call ");
	return false;
}

static bool do_return(CallFrame **fr) {
	CallFrame *frame = *fr;
	Value result = vm_pop();
	close_upvalues(frame->slots);
	vm.running->frame_count--;
	vm.running->stack_top -= frame->closure->function->arity + 1;

	if (vm.running->frame_count == 0) {
		vm.running->state = COROUTINE_COMPLETE;
		if (vm.running->parent) {
			vm.running = vm.running->parent;
			vm.running->stack_top--;
		} else {
#ifdef DEBUG_TRACE_EXECUTION
			printf("stack:  ");
			for (Value* slot = vm.running->stack; slot < vm.running->stack_top; slot++) {
				printf("[ ");
				value_print(*slot);
				printf(" ]");
			}
			printf("\n");
#endif
			return true;
		}
		*fr = &vm.running->frames[vm.running->frame_count - 1];
		// vm.running->stack_top = frame->slots;
	} else {
		*fr = &vm.running->frames[vm.running->frame_count - 1];
	}
	vm.running->current_frame = frame;

	vm_push(result);
	return false;
}

static InterpretResult run() {
	// TODO: Implement register ip optimization
	// register uint8_t *ip = vm.chunk->code;
	CallFrame *frame = &vm.running->frames[vm.running->frame_count - 1];

  #define READ_BYTE() (*frame->ip++)
	#define READ_WORD() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
	#define READ_DWORD() (frame->ip += 4, (uint32_t)((frame->ip[-4] << 24) | (frame->ip[-3] << 16) | (frame->ip[-2] << 8) | frame->ip[-1]))
  #define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
	#define READ_CONSTANT_LONG()    \
		(frame->closure->function->chunk.constants.values[READ_BYTE() | (READ_BYTE() << 8) | (READ_BYTE() << 16)])

	#ifdef DYNAMIC_TYPE_CHECKING
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
	#else
	#define BINARY_OP(value_type, op) \
		do { \
			double b = AS_NUMBER(vm_pop()); \
			double a = AS_NUMBER(vm_pop()); \
			vm_push(value_type(a op b)); \
		} while (false)
	#endif

	#ifdef DEBUG_TRACE_EXECUTION
	printf("== trace ==\n");
	#endif

	for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
		printf("stack:  ");
		for (Value* slot = vm.running->stack; slot < vm.running->stack_top; slot++) {
			printf("[ ");
			value_print(*slot);
			printf(" ]");
		}
		printf("\n");
		printf("top:    ");
		printf("[ ");
		value_print(vm.running->stack_top[-1]);
		printf(" ]");
		printf("\n");
		printf("ip:     ");
		printf("[ ");
		printf("%zu", (size_t)(frame->ip - frame->closure->function->chunk.code));
		// printf("%p", (frame->ip));
		printf(" ]");
		printf("\n");
		disassemble_instruction(&frame->closure->function->chunk, (size_t)(frame->ip - frame->closure->function->chunk.code));
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
			if (
				IS_STRING(vm_peek(0))
				// We only need to check the first operand if safety checks are disabled
#ifdef DYNAMIC_TYPE_CHECKING
				&& IS_STRING(vm_peek(1))
#endif
				) {
				concatonate();
			} else if (
				IS_NUMBER(vm_peek(0))
#ifdef DYNAMIC_TYPE_CHECKING
				&& IS_NUMBER(vm_peek(1))
#endif
				) {
				double b = AS_NUMBER(vm_pop());
				double a = AS_NUMBER(vm_pop());
				vm_push(NUMBER_VAL(a + b));
			} else {
				runtime_error( "Operands must be two numbers or two strings.");
				return INTERPRET_RUNTIME_ERROR;
			}
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
#ifdef DYNAMIC_TYPE_CHECKING
			if (!IS_NUMBER(vm_peek(0))) {
				runtime_error("Operand must be a number");
				return INTERPRET_RUNTIME_ERROR;
			}
#endif
			vm_push(NUMBER_VAL(-AS_NUMBER(vm_pop())));
			break;
		}
		case OP_CALL: {
			size_t argc = READ_BYTE();
			// TODO: better error handling
			if (!call_value(vm_peek(argc), argc)) {
				return INTERPRET_RUNTIME_ERROR;
			}
			frame = &vm.running->frames[vm.running->frame_count - 1];
			break;
		}
		case OP_SET_FIELD: {
			Value value = vm_pop();
			Value key = vm_pop();
			Value container = vm_pop();

			if (!set_field(container, key, value)) {
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_GET_FIELD: {
			Value key = vm_pop();
			Value container = vm_pop();

			if (!get_field(container, key)) {
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_LIST: {
			List *list = list_new();
			uint8_t count = READ_BYTE();
			while (count--) {
				list_push(list, vm_pop());
			}
			vm_push(OBJ_VAL(list));
			break;
		}
		case OP_LIST_LONG: {
			List *list = list_new();
			uint32_t count = READ_BYTE();
			count |= READ_BYTE() << 8;
			count |= READ_BYTE() << 16;
			while (count--) {
				list_push(list, vm_pop());
			}
			vm_push(OBJ_VAL(list));
			break;
		}
		case OP_DICT: {
			Dictionary *dict = dict_new();
			uint8_t count = READ_BYTE();
			while (count--) {
				Value value = vm_pop();
				Value key = vm_pop(); // because of the compiler, this should *always* be a string
				dict_set(dict, AS_STRING(key), value);
			}
			vm_push(OBJ_VAL(dict));
			break;
		}
		case OP_DICT_LONG: {
			Dictionary *dict = dict_new();
			uint32_t count = READ_BYTE();
			count |= READ_BYTE() << 8;
			count |= READ_BYTE() << 16;
			while (count--) {
				Value value = vm_pop();
				Value key = vm_pop(); // same as above
				dict_set(dict, AS_STRING(key), value);
			}
			vm_push(OBJ_VAL(dict));
			break;
		}
		case OP_CLOSURE: {
			Function *function = AS_FUNCTION(READ_CONSTANT());
			Closure *closure = closure_new(function);
			vm_push(OBJ_VAL(closure));
			for (uint8_t i = 0; i < closure->function->upvalue_count; i++) {
				uint8_t is_local = READ_BYTE();
				uint8_t index = READ_BYTE();
				if (is_local) {
					closure->upvalues[i] = upvalue_capture(&frame->slots[index]);
				} else {
					closure->upvalues[i] = frame->closure->upvalues[index];
				}
			}
			break;
		}
		case OP_CLOSURE_LONG: {
			Function *function = AS_FUNCTION(READ_CONSTANT_LONG());
			Closure *closure = closure_new(function);

			vm_push(OBJ_VAL(closure));
			for (uint8_t i = 0; i < closure->function->upvalue_count; i++) {
				uint8_t is_local = READ_BYTE();
				uint8_t index = READ_BYTE();
				if (is_local) {
					closure->upvalues[i] = upvalue_capture(&frame->slots[index]);
				} else {
					closure->upvalues[i] = frame->closure->upvalues[index];
				}
			}
			break;
		}
		case OP_GET_UPVALUE: {
			uint8_t index = READ_BYTE();
			vm_push(*frame->closure->upvalues[index]->location);
			break;
		}
		case OP_SET_UPVALUE: {
			uint8_t index = READ_BYTE();
			*frame->closure->upvalues[index]->location = vm_peek(0);
			break;
		}
		case OP_CLOSE_UPVALUE:
			close_upvalues(vm.running->stack_top - 1);
			vm_pop();
			break;
		case OP_RETURN: {
			if (do_return(&frame)) {
				return INTERPRET_OK;
			}
			break;
		}
		case OP_POP: {
			vm_pop();
			break;
		}
		case OP_DEFINE_GLOBAL: {
			String *name = AS_STRING(READ_CONSTANT());
			table_set(&vm.globals, name, vm_peek(0));
			vm_pop();
			break;
		}
		case OP_DEFINE_GLOBAL_LONG: {
			String *name = AS_STRING(READ_CONSTANT_LONG());
			table_set(&vm.globals, name, vm_peek(0));
			vm_pop();
			break;
		}
		case OP_SET_GLOBAL: {
			String *name = AS_STRING(READ_CONSTANT());
			if (table_set(&vm.globals, name, vm_peek(0))) {
				table_delete(&vm.globals, name);
				// TODO: what should I do here?
				runtime_error("Undefined variable '%.*s'.", name->length, name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_SET_GLOBAL_LONG: {
			String *name = AS_STRING(READ_CONSTANT_LONG());
			if (table_set(&vm.globals, name, vm_peek(0))) {
				table_delete(&vm.globals, name);
				// TODO: same as above
				runtime_error("Undefined variable '%.*s'.", name->length, name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_GET_GLOBAL: {
			String *name = AS_STRING(READ_CONSTANT());
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
			String *name = AS_STRING(READ_CONSTANT_LONG());
			Value value;
			if (!table_get(&vm.globals, name, &value)) {
				// Same as above
				value = NIL_VAL;
			}
			vm_push(value);
			break;
		}
		case OP_COROUTINE: {
			Value f = vm_peek(0);
			if (!IS_CLOSURE(f)) {
				runtime_error("Attempted to create a coroutine from a non-function value.");
				return INTERPRET_RUNTIME_ERROR;
			}
			Coroutine *co = coroutine_new(AS_CLOSURE(f));
			vm_pop();
			vm_push(OBJ_VAL(co));
			break;
		}
		case OP_GET_LOCAL: {
			uint8_t slot = READ_BYTE();
			vm_push(frame->slots[slot]);
			break;
		}
		case OP_GET_LOCAL_LONG: {
			uint32_t slot = READ_BYTE();
			slot |= READ_BYTE() << 8;
			slot |= READ_BYTE() << 16;
			vm_push(frame->slots[slot]);
			break;
		}
		case OP_SET_LOCAL: {
			uint8_t slot = READ_BYTE();
			frame->slots[slot] = vm_peek(0);
			break;
		}
		case OP_SET_LOCAL_LONG: {
			uint32_t slot = READ_BYTE();
			slot |= READ_BYTE() << 8;
			slot |= READ_BYTE() << 16;
			frame->slots[slot] = vm_peek(0);
			break;
		}
		case OP_JUMP: {
			frame->ip += READ_DWORD();
			break;
		}
		case OP_JUMP_IF_FALSE: {
			uint32_t offset = READ_DWORD();
			frame->ip += (value_is_falsy(vm_peek(0)) * offset);
			break;
		}
		case OP_LOOP: {
			uint32_t offset = READ_DWORD();
			frame->ip -= offset;
			break;
		}
		}
	}

  #undef READ_BYTE
	#undef READ_WORD
	#undef READ_DWORD
  #undef READ_CONSTANT
	#undef READ_CONSTANT_LONG
	#undef BINARY_OP
}

InterpretResult vm_interpret(Function *function) {
	vm_push(OBJ_VAL(function));
	Closure *closure = closure_new(function);
	vm_pop();
	vm_push(OBJ_VAL(closure));
	call(closure, 0);

	return run();
}
