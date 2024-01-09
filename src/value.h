#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef enum value_type_t {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
} value_type_t;

typedef struct {
  value_type_t type;
  union {
    bool boolean;
    double number;
  } as;
} value_t;

#define BOOL_VAL(value) ((value_t){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((value_t){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((value_t){VAL_NUMBER, {.number = value}})

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

#define IS_FALSY(value) (IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)))

typedef struct {
  size_t count;
  size_t capacity;
  value_t *values;
} value_array_t;

void value_array_init(value_array_t *array);
void value_array_write(value_array_t *array, value_t value);
void value_array_free(value_array_t *array);

void value_print(value_t value);
void value_println(value_t value);

bool value_equal(value_t a, value_t b);

#endif
