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

void value_print_indented(Value value, int indent) {
	for (int i = 0; i < indent; i++) {
		printf("  ");
	}

	if (IS_BOOL(value)) {
		printf(AS_BOOL(value) ? "true" : "false");
	} else if (IS_NIL(value)) {
		printf("nil");
	} else if (IS_NUMBER(value)) {
		printf("%g", AS_NUMBER(value));
	} else if (IS_OBJ(value)) {
		object_print_indented(value, indent);
	}
}

void value_print(Value value) {
#ifdef NAN_BOXING
	if (IS_BOOL(value)) {
		printf(AS_BOOL(value) ? "true" : "false");
	} else if (IS_NIL(value)) {
		printf("nil");
	} else if (IS_NUMBER(value)) {
		printf("%g", AS_NUMBER(value));
	} else if (IS_OBJ(value)) {
		object_print(value);
	}
#else
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
#endif
}

void value_println(Value value) {
	value_print(value);
	printf("\n");
}

bool value_equal(Value a, Value b) {
#ifdef NAN_BOXING
	return a == b;
#else
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
#endif
}

bool value_is_falsy(Value value) {
	return IS_NIL(value) || (IS_BOOL(value) &&!AS_BOOL(value));
}

ValueType value_type(Value value) {
	if (IS_OBJ(value)) {
		return VAL_OBJ;
	}

	if (IS_NUMBER(value)) {
		return VAL_NUMBER;
	}

	if (IS_BOOL(value)) {
		return VAL_BOOL;
	}

	return VAL_NIL;
}

const ConstStr value_type_name(Value value) {
	switch (value_type(value)) {
	case VAL_BOOL:
		return CONST_STR(bool);
	case VAL_NIL:
		return CONST_STR(nil);
	case VAL_NUMBER:
		return CONST_STR(number);
	case VAL_OBJ:
		switch (object_type(AS_OBJ(value))) {
		case OBJ_STRING:
			return CONST_STR(string);
		case OBJ_FUNCTION:
			return CONST_STR(function);
		case OBJ_CLOSURE:
			return CONST_STR(closure);
		case OBJ_UPVALUE:
			return value_type_name(AS_UPVALUE(value)->closed);
		case OBJ_NATIVE:
			return CONST_STR(native);
		case OBJ_LIST:
			return CONST_STR(list);
		case OBJ_DICT:
			return CONST_STR(dict);
		case OBJ_COROUTINE:
			return CONST_STR(coroutine);
		}
	}
}

bool value_is_of_type(Value value, ValueType type) {
	switch(type) {
	case VAL_BOOL:
		return IS_BOOL(value);
	case VAL_NIL:
		return IS_NIL(value);
	case VAL_NUMBER:
		return IS_NUMBER(value);
	case VAL_OBJ:
		return IS_OBJ(value);
	}
}
