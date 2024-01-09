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

static object_string_t* alloc_string(char *chars, size_t length, bool owned) {
	object_string_t *str = ALLOCATE_OBJ(object_string_t, OBJ_STRING, owned);
	str->length = length;
	str->chars  = chars;
	return str;
}

object_string_t* copy_string(const char *chars, size_t length) {
	char *heap_chars = ALLOCATE(char, length + 1);
	memcpy(heap_chars, chars, length);
	heap_chars[length] = '\0';
	return alloc_string(heap_chars, length, true);
}

object_string_t* ref_string(char *chars, size_t length) {
	return alloc_string(chars, length, false);
}

object_string_t *take_string(char *chars, size_t length) {
	return alloc_string(chars, length, true);
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
