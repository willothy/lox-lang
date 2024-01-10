#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef enum ValueType {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ
} ValueType;

typedef struct Object Object;
typedef struct ObjectString ObjectString;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Object *object;
  } as;
} Value;

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(obj) ((Value){VAL_OBJ, {.object = (Object *)obj}})

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value) ((value).as.object)

#define OBJ_TYPE(object) (AS_OBJ(object)->type)

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value) ((value).type == VAL_OBJ)

#define IS_FALSY(value) (IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)))

typedef struct {
  size_t count;
  size_t capacity;
  Value *values;
} ValueArray;

void value_array_init(ValueArray *array);
void value_array_write(ValueArray *array, Value value);
void value_array_free(ValueArray *array);

void value_print(Value value);
void value_println(Value value);

bool value_equal(Value a, Value b);

#endif
