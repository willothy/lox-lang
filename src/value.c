#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

void value_array_init(value_array_t *array) {
	array->values = NULL;
	array->capacity = 0;
	array->count = 0;
}

void value_array_write(value_array_t *array, value_t value) {
	if (array->capacity < array->count + 1) {
		size_t old_capacity = array->capacity;
		array->capacity =  GROW_CAPACITY(old_capacity);
		array->values = GROW_ARRAY(value_t, array->values, old_capacity, array->capacity);
	}

	array->values[array->count] = value;
	array->count++;
}

void value_array_free(value_array_t *array) {
	FREE_ARRAY(value_t, array->values, array->capacity);
	value_array_init(array);
}

void value_print(value_t value) {
	switch (value.type) {
	case VAL_NUMBER:
		printf("%g", AS_NUMBER(value));
		break;
	case VAL_BOOL:
		printf("%s", AS_BOOL(value) ? "true" : "false");
		break;
	case VAL_NIL:
		printf("nil");
		break;
	case VAL_OBJ:
		object_print(value);
		break;
	}
}

void value_println(value_t value) {
	printf("%g\n", AS_NUMBER(value));
}

bool value_equal(value_t a, value_t b) {
	if (a.type != b.type){
		return false;
	}

	switch (a.type) {
	case VAL_NUMBER:
		return AS_NUMBER(a) == AS_NUMBER(b);
	case VAL_BOOL:
		return AS_BOOL(a) == AS_BOOL(b);
	case VAL_OBJ:
		return AS_OBJ(a) == AS_OBJ(b);
	case VAL_NIL:
		return true;
	default:
		return false;
	}
}
