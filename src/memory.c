#include <stdlib.h>

#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

void *reallocate(void *ptr, size_t old_size, size_t new_size) {
#ifdef DEBUG_STRESS_GC
	if (new_size > old_size) {
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

void collect_garbage() {
#ifdef DEBUG_LOG_GC
	printf("-- gc begin\n");
#endif

#ifdef DEBUG_LOG_GC
	printf("-- gc end\n");
#endif
}

static void free_object(Object *obj) {
#ifdef DEBUG_LOG_GC
	printf("%p free type %s\n", (void *)obj, object_type_name(obj->type));
#endif

	switch (obj->type) {
	case OBJ_STRING: {
		ObjectString *str = (ObjectString *)obj;
		if (obj->owned) {
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

void free_objects() {
	Object *obj = vm.objects;
	while (obj) {
		Object *next = obj->next;
		free_object(obj);
		obj = next;
	}
	vm.objects = NULL;
}
