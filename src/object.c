#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, obj_type, owned) \
	(type *)allocate_object(sizeof(type), obj_type, owned)

static Object* allocate_object(size_t size, ObjectType type, bool owned) {
#ifdef DEBUG_LOG_GC
	printf("%p allocate %zu for %s\n", (void *)vm.objects, size, object_type_name(type));
#endif

	Object *obj = (Object *)reallocate(NULL, 0, size);
	// obj->header = (uint64_t)vm.objects | (uint64_t)type << 56 | (uint64_t)owned << 57;
	obj->header = (uint64_t)vm.objects << 16
	              | (uint64_t)owned << 9
	              | (uint64_t)vm.mark_value << 8
	              | (uint64_t)type;
	vm.objects = obj;
	return obj;
}

const char *object_type_name(ObjectType type) {
	switch(type) {
	case OBJ_STRING:
		return "string";
	case OBJ_CLOSURE:
		return "closure";
	case OBJ_FUNCTION:
		return "function";
	case OBJ_NATIVE:
		return "native";
	case OBJ_LIST:
		return "list";
	case OBJ_DICT:
		return "dict";
	case OBJ_UPVALUE:
		return "upvalue";
	case OBJ_COROUTINE:
		return "coroutine";
	}
}

// fnv-1a hash function
static uint32_t hash_string(const char *key, size_t length) {
	uint32_t hash = 2166136261u;
	for(size_t i = 0; i < length; i++) {
		hash ^= key[i];
		hash *= 16777619;
	}
	return hash;
}

static String* alloc_string(char *chars, size_t length, uint32_t hash, bool owned) {
	String *str = ALLOCATE_OBJ(String, OBJ_STRING, owned);
	str->length = length;
	str->chars  = chars;
	str->hash   = hash;
	vm_push(OBJ_VAL(str));
	table_set(&vm.strings, str, NIL_VAL);
	vm_pop();
	return str;
}

void string_print(String *str) {
	printf("\"%.*s\"", (int)str->length, str->chars);
}

String* copy_string(const char *chars, size_t length) {
	uint32_t hash = hash_string(chars, length);
	String *interned = table_find_string(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		return interned;
	}

	char *heap_chars = ALLOCATE(char, length + 1);
	memcpy(heap_chars, chars, length);
	heap_chars[length] = '\0';
	return alloc_string(heap_chars, length, hash, true);
}

String *const_string(const char *chars, size_t length) {
	uint32_t hash = hash_string(chars, length);
	String *interned = table_find_string(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		// If the string is already interned, we should use the interned version
		// to ensure equality checks work correctly.
		//
		// Because the string's memory is not owned by the VM, we do not need to free it.
		return interned;
	}
	return alloc_string((char*)chars, length, hash, false);
}

String* ref_string(char *chars, size_t length) {
	return copy_string(chars, length); // temporary bc we don't have GC to manage chunks
	// uint32_t hash = hash_string(chars, length);
	// ObjectString *interned = table_find_string(&vm.strings, chars, length, hash);
	// if (interned != NULL) {
	// 	// If the string is already interned, we should use the interned version
	// 	// to ensure equality checks work correctly.
	// 	//
	// 	// Because the string's memory is not owned by the VM, we do not need to free it.
	// 	return interned;
	// }
	// return alloc_string(chars, length, hash, false);
}

String *take_string(char *chars, size_t length) {
	uint32_t hash = hash_string(chars, length);
	String *interned = table_find_string(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		FREE_ARRAY(char, chars, length + 1);
		return interned;
	}
	return alloc_string(chars, length, hash, true);
}

void object_print_indented(Value val, int depth) {
	for (int i = 0; i < depth; i++) {
		printf("  ");
	}

	switch(OBJ_TYPE(val)) {
	case OBJ_STRING:
		// Lox strings are not null-terminated and can be non-owned references to memory
		// outside of the VM's heap. To print them, we need to use printf's precision
		// specifier to print only the characters in the string.
		printf("%.*s", (int)AS_STRING(val)->length, AS_CSTRING(val));
		break;
	case OBJ_CLOSURE:
		function_print(AS_CLOSURE(val)->function);
		break;
	case OBJ_FUNCTION:
		function_print(AS_FUNCTION(val));
		break;
	case OBJ_NATIVE:
		printf("<native fn>");
		break;
	case OBJ_UPVALUE: // unreachable
		printf("<upvalue>");
		break;
	case OBJ_COROUTINE:
		printf("<coroutine>");
		break;
	case OBJ_LIST: {
		ValueArray *list = &AS_LIST(val)->values;
		size_t count = list->count;
		if (count > 1) {
			printf("[\n");
		} else {
			printf("[");
		}
		int elem_depth = count > 1 ? depth + 1 : depth;
		for (int i = 0; i < count; i++) {
			value_print_indented(list->values[i], elem_depth);
			if (i < count - 1) {
				printf(",\n");
			}
		}
		if (count > 1) {
			printf("\n]");
		} else  {
			printf("]");
		}
		break;
	}
	case OBJ_DICT: {
		Table *list = &AS_DICT(val)->table;
		size_t count = list->count;
		printf("{\n");
		size_t found = 0;
		for (int i = 0; i < list->capacity; i++) {
			Entry *entry = &list->entries[i];
			if (entry->key == NULL || IS_NIL(entry->value)) {
				continue;
			}
			value_print_indented(OBJ_VAL(entry->key), depth + 1);
			printf(": ");
			value_print_indented(entry->value, depth);
			printf(",\n");
		}
		printf("}");
		break;
	}
	}
}

void object_print(Value val) {
	object_print_indented(val, 0);
}

Function *function_new() {
	Function *function = ALLOCATE_OBJ(Function, OBJ_FUNCTION, true);
	function->arity = 0;
	function->name = NULL;
	function->upvalue_count = 0;
	chunk_init(&function->chunk);
	return function;
}

void function_print(Function *function) {
	if (function->name == NULL) {
		printf("<script>");
		return;
	}
	if (function->name->length == 0){
		printf("<fn>");
		return;
	}
	printf("<fn %.*s>", (int)function->name->length, function->name->chars);
}

NativeFunction *native_new(NativeFnPtr function, uint8_t arity) {
	NativeFunction *native = ALLOCATE_OBJ(NativeFunction, OBJ_NATIVE, true);
	native->function = function;
	native->arity = arity;
	return native;
}

Closure *closure_new(Function *function) {
	Upvalue **upvalues = ALLOCATE(Upvalue*, function->upvalue_count);

	for (int i = 0; i < function->upvalue_count; i++) {
		upvalues[i] = NULL;
	}

	Closure *closure = ALLOCATE_OBJ(Closure, OBJ_CLOSURE, true);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalue_count = function->upvalue_count;
	return closure;
}

Upvalue *upvalue_new(Value *slot) {
	Upvalue *upvalue = ALLOCATE_OBJ(Upvalue, OBJ_UPVALUE, true);
	upvalue->location = slot;
	upvalue->closed = NIL_VAL;
	upvalue->next = NULL;
	return upvalue;
}


Value list_get(List *list, size_t index) {
	if (index >= list->values.count) {
		return NIL_VAL;
	}

	return list->values.values[index];
}

size_t list_length(List *list) {
	return list->values.count;
}

void list_set(List *list, size_t index, Value value) {
	if (index >= list->values.count) {
		// FIXME: either error or grow table and fill with nils
		return;
	}
	list->values.values[index] = value;
}

Value list_remove(List *list, size_t index) {
	if (index >= list->values.count) {
		return NIL_VAL;
	}
	Value value = list->values.values[index];
	for (size_t i = index; i < list->values.count - 1; i++) {
		list->values.values[i] = list->values.values[i + 1];
	}
	list->values.count--;
	return value;
}

void list_push(List *list, Value value) {
	value_array_write(&list->values, value);
}

Value list_pop(List *list) {
	if (list->values.count == 0) {
		return NIL_VAL;
	}
	Value value = list->values.values[list->values.count - 1];
	list->values.values[list->values.count - 1] = NIL_VAL; // is this necessary?
	list->values.count--;
	return value;
}

List *list_new() {
	List *list = ALLOCATE_OBJ(List, OBJ_LIST, true);
	value_array_init(&list->values);
	return list;
}

Dictionary *dict_new() {
	Dictionary *dict = ALLOCATE_OBJ(Dictionary, OBJ_DICT, true);
	table_init(&dict->table);
	return dict;
}

void dict_set(Dictionary *dict, String *key, Value value) {
	table_set(&dict->table, key, value);
}

Value dict_get(Dictionary *dict, String *key) {
	Value value = NIL_VAL;
	table_get(&dict->table, key, &value);
	return value;
}

Value dict_remove(Dictionary *dict, String *key) {
	Value value = NIL_VAL;
	table_get_and_delete(&dict->table, key, &value);
	return value;
}

void dict_clear(Dictionary *dict) {
	dict->table.count = 0;
}

Coroutine *coroutine_new(Closure *closure) {
	Coroutine *coroutine = ALLOCATE_OBJ(Coroutine, OBJ_COROUTINE, true);

	coroutine->stack = NULL;
	coroutine->stack = GROW_ARRAY(Value, coroutine->stack, 0, STACK_INITIAL);
	coroutine->stack_size = STACK_INITIAL;
	coroutine->stack_top = coroutine->stack;

	coroutine->frames = NULL;
	coroutine->frames = GROW_ARRAY(CallFrame, coroutine->frames, 0, FRAMES_INITIAL);
	coroutine->frame_capacity = FRAMES_INITIAL;

	if (closure) {
		CallFrame *frame = &coroutine->frames[0];
		frame->closure = closure;
		frame->ip = closure->function->chunk.code;
		frame->slots = coroutine->stack_top - closure->function->arity + 1;

		coroutine->frame_count = 1;
		coroutine->current_frame = frame;
	} else {
		coroutine->frame_count = 0;
		coroutine->current_frame = NULL;
	}

	// parent will be set when the coroutine is started / resumed
	coroutine->parent = NULL;

	coroutine->state = COROUTINE_READY;

	return coroutine;
}

void coroutine_push(Coroutine *coroutine, Value value) {
	if (coroutine->stack_top - coroutine->stack >= coroutine->stack_size) {
		size_t old_size = coroutine->stack_size;
		size_t offset = coroutine->stack_top - coroutine->stack;
		coroutine->stack_size = GROW_CAPACITY(old_size);
		coroutine->stack = GROW_ARRAY(Value, coroutine->stack, old_size, coroutine->stack_size);
		coroutine->stack_top = coroutine->stack + offset;
	}
	*coroutine->stack_top = value;
	coroutine->stack_top++;
}

Value coroutine_pop(Coroutine *coroutine) {
	coroutine->stack_top--;
	return *coroutine->stack_top;
}

Value coroutine_peek(Coroutine *coroutine, size_t distance) {
	return coroutine->stack_top[-1 - distance];
}

void coroutine_reset(Coroutine *coroutine) {
	coroutine->stack_top = coroutine->stack;

	if (coroutine->frame_count > 0) {
		coroutine->frame_count = 1;
		coroutine->current_frame = &coroutine->frames[0];
		coroutine->current_frame->ip = coroutine->current_frame->closure->function->chunk.code;
	} else {
		coroutine->frame_count = 0;
		coroutine->current_frame = NULL;
	}

	coroutine->state = COROUTINE_READY;
}
