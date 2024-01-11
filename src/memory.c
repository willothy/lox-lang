#include <stdlib.h>

#include "memory.h"
#include "compiler.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(void *ptr, size_t old_size, size_t new_size) {
	vm.bytes_allocated += new_size - old_size;

#ifdef DEBUG_STRESS_GC
	if (new_size > old_size) {
		collect_garbage();
	}
#else
	if (vm.bytes_allocated > vm.next_gc) {
		collect_garbage();
	}
#endif

	if (new_size == 0) {
		free(ptr);
		return NULL;
	}
	void *rv = realloc(ptr, new_size);
	if (rv == NULL) {
		exit(1);
	}
	return rv;
}

static void free_object(Object *obj) {
	if (obj == NULL) {
		return;
	}
#ifdef DEBUG_LOG_GC
	printf("%p free type %s\n", (void *)obj, object_type_name(obj->type));
#endif

	switch (object_type(obj)) {
	case OBJ_STRING: {
		ObjectString *str = (ObjectString *)obj;
		if (object_is_owned(obj)) {
			FREE_ARRAY(char, str->chars, str->length + 1);
		}
		FREE(ObjectString, obj);
		break;
	}
	case OBJ_CLOSURE: {
		ObjectClosure *closure = (ObjectClosure*)obj;
		FREE_ARRAY(ObjectUpvalue*, closure->upvalues, closure->upvalue_count);
		FREE(ObjectClosure, obj);
		break;
	}
	case OBJ_FUNCTION: {
		ObjectFunction *fn = (ObjectFunction *)obj;
		chunk_free(&fn->chunk);
		FREE(ObjectFunction, obj);
		break;
	}
	case OBJ_UPVALUE: {
		FREE(ObjectUpvalue,obj);
		break;
	}
	case OBJ_NATIVE: {
		FREE(ObjectNative, obj);
		break;
	}
	}
}

void mark_object(Object *obj) {
	if (obj == NULL || object_is_marked(obj) == vm.mark_value) {
		return;
	}

#ifdef DEBUG_LOG_GC
	printf("%p mark ", (void *)obj);
	value_println(OBJ_VAL(obj));
#endif
	if (vm.mark_value) {
		object_mark(obj);
	} else {
		object_unmark(obj);
	}

	if (vm.gray_capacity < vm.gray_count + 1) {
		vm.gray_capacity = GROW_CAPACITY(vm.gray_capacity);
		vm.gray_stack = (Object**)realloc(vm.gray_stack, sizeof(Object *) * vm.gray_capacity);
		if (vm.gray_stack == NULL) {
			exit(1);
		}
	}

	vm.gray_stack[vm.gray_count++] = obj;
}

void mark_value(Value value) {
	if (!IS_OBJ(value)) {
		return;
	}
	mark_object(AS_OBJ(value));
}

static void mark_roots() {
	for (Value *slot = vm.stack; slot < vm.stack_top; slot++) {
		mark_value(*slot);
	}

	for (size_t i = 0; i < vm.frame_count; i++) {
		mark_object((Object *)vm.frames[i].closure);
	}

	for (ObjectUpvalue *upvalue = vm.open_upvalues; upvalue!= NULL; upvalue = upvalue->next) {
		mark_object((Object *)upvalue);
	}

	table_mark(&vm.globals);

	compiler_mark_roots();
}

static void mark_array(ValueArray *array) {
	for (size_t i = 0; i < array->count; i++) {
		mark_value(array->values[i]);
	}
}

static void blacken_object(Object *obj) {
#ifdef DEBUG_LOG_GC
	printf("%p blacken ", (void *)obj);
	value_println(OBJ_VAL(obj));
#endif

	switch (object_type(obj)) {
	case OBJ_FUNCTION: {
		ObjectFunction *fn = (ObjectFunction *)obj;
		mark_object((Object *)fn->name);
		mark_array(&fn->chunk.constants);
		break;
	}
	case OBJ_CLOSURE: {
		ObjectClosure *closure = (ObjectClosure *)obj;
		mark_object((Object *)closure->function);
		for (size_t i = 0; i < closure->upvalue_count; i++) {
			mark_object((Object *)closure->upvalues[i]);
		}
		break;
	}
	case OBJ_UPVALUE:
		mark_value(((ObjectUpvalue *)obj)->closed);
		break;
	case OBJ_STRING:
	case OBJ_NATIVE:
		break;
	}
}

static void trace_references() {
	while (vm.gray_count > 0) {
		Object *obj = vm.gray_stack[--vm.gray_count];
		blacken_object(obj);
	}
}

static void sweep() {
	Object *prev = NULL;
	Object *obj = vm.objects;
	while (obj != NULL) {
		if (object_is_marked(obj) == vm.mark_value) {
			prev = obj;
			obj = object_next(obj);
		} else {
			Object *unreached = obj;
			obj = object_next(obj);

			if (prev != NULL) {
				object_set_next(prev, obj);
			} else {
				vm.objects = obj;
			}

			free_object(unreached);
		}
	}
}

void collect_garbage() {
#ifdef DEBUG_LOG_GC
	printf("-- gc begin\n");
#endif
	size_t before = vm.bytes_allocated;

	mark_roots();
	trace_references();
	table_remove_white(&vm.strings);
	sweep();

	// Don't increase the threshold if no memory was freed.
	if (vm.bytes_allocated < before) {
		vm.next_gc = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;
	}

	vm.mark_value = !vm.mark_value;

#ifdef DEBUG_LOG_GC
	printf("-- gc end\n");
	printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
	       before - vm.bytes_allocated, before, vm.bytes_allocated,
	       vm.next_gc);
	printf("   heap capacity: %zu\n", vm.bytes_allocated);
#endif
}

void free_objects() {
	Object *obj = vm.objects;
	while (obj) {
		Object *next = object_next(obj);
		free_object(obj);
		obj = next;
	}
	vm.objects = NULL;
	free(vm.gray_stack);
}
