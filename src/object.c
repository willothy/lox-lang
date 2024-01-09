#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

#define ALLOCATE_OBJ(type, obj_type) \
	(type *)allocate_object(sizeof(type), obj_type)

static object_t* allocate_object(size_t size, object_type_t type) {
	object_t *obj = (object_t *)reallocate(NULL, 0, size);
	obj->type = type;
	return obj;
}

static object_string_t* alloc_string(char *chars, size_t length) {
	object_string_t *str = ALLOCATE_OBJ(object_string_t, OBJ_STRING);
	str->length = length;
	str->chars  = chars;
	return str;
}

object_string_t* copy_string(const char *chars, size_t length) {
	char *heap_chars = ALLOCATE(char, length + 1);
	memcpy(heap_chars, chars, length);
	heap_chars[length] = '\0';
	return alloc_string(heap_chars, length);
}

void object_print(value_t val) {
	switch(OBJ_TYPE(val)) {
	case OBJ_STRING:
		printf("%s", AS_CSTRING(val));
		break;
	}
}
