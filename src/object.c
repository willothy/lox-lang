#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, obj_type, owned) \
	(type *)allocate_object(sizeof(type), obj_type, owned)

static object_t* allocate_object(size_t size, object_type_t type, bool owned) {
	object_t *obj = (object_t *)reallocate(NULL, 0, size);
	obj->type = type;
	obj->owned = owned;
	obj->next = vm.objects;
	vm.objects = obj;
	return obj;
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

static object_string_t* alloc_string(char *chars, size_t length, uint32_t hash, bool owned) {
	object_string_t *str = ALLOCATE_OBJ(object_string_t, OBJ_STRING, owned);
	str->length = length;
	str->chars  = chars;
	str->hash   = hash;
	table_set(&vm.strings, str, NIL_VAL);
	return str;
}

object_string_t* copy_string(const char *chars, size_t length) {
	uint32_t hash = hash_string(chars, length);
	object_string_t *interned = table_find_string(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		return interned;
	}

	char *heap_chars = ALLOCATE(char, length + 1);
	memcpy(heap_chars, chars, length);
	heap_chars[length] = '\0';
	return alloc_string(heap_chars, length, hash, true);
}

object_string_t* ref_string(char *chars, size_t length) {
	return copy_string(chars, length); // temporary bc we don't have GC to manage chunks
	// uint32_t hash = hash_string(chars, length);
	// object_string_t *interned = table_find_string(&vm.strings, chars, length, hash);
	// if (interned != NULL) {
	// 	// If the string is already interned, we should use the interned version
	// 	// to ensure equality checks work correctly.
	// 	//
	// 	// Because the string's memory is not owned by the VM, we do not need to free it.
	// 	return interned;
	// }
	// return alloc_string(chars, length, hash, false);
}

object_string_t *take_string(char *chars, size_t length) {
	uint32_t hash = hash_string(chars, length);
	object_string_t *interned = table_find_string(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		FREE_ARRAY(char, chars, length + 1);
		return interned;
	}
	return alloc_string(chars, length, hash, true);
}

void object_print(value_t val) {
	switch(OBJ_TYPE(val)) {
	case OBJ_STRING:
		// Lox strings are not null-terminated and can be non-owned references to memory
		// outside of the VM's heap. To print them, we need to use printf's precision
		// specifier to print only the characters in the string.
		printf("%.*s", (int)AS_STRING(val)->length, AS_CSTRING(val));
		break;
	}
}
