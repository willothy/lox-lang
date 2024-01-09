#include <stdio.h>

#include "memory.h"
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
	printf("%g", value);
}

void value_println(value_t value) {
	printf("%g\n", value);
}
