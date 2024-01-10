#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

void value_array_init(ValueArray *array) {
	array->values = NULL;
	array->capacity = 0;
	array->count = 0;
}

void value_array_write(ValueArray *array, Value value) {
	if (array->capacity < array->count + 1) {
		size_t old_capacity = array->capacity;
		array->capacity =  GROW_CAPACITY(old_capacity);
		array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
	}

	array->values[array->count] = value;
	array->count++;
}

void value_array_free(ValueArray *array) {
	FREE_ARRAY(Value, array->values, array->capacity);
	value_array_init(array);
}

void value_print(Value value) {
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

void value_println(Value value) {
	value_print(value);
	printf("\n");
}

bool value_equal(Value a, Value b) {
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

bool value_is_falsy(Value value) {
	return IS_NIL(value) || (IS_BOOL(value) &&!AS_BOOL(value));
}
