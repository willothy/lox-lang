#ifndef clox_value_h
#define clox_value_h

#include "common.h"
#include <string.h>

typedef enum {
  OBJ_STRING = 0,
  OBJ_FUNCTION,
  OBJ_CLOSURE,
  OBJ_UPVALUE,
  OBJ_NATIVE,
  OBJ_LIST,
  OBJ_DICT,
  OBJ_COROUTINE,
} ObjectType;

typedef enum ValueType {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ,
} ValueType;

typedef struct Object Object;
typedef struct String String;

typedef struct {
  const char *chars;
  size_t length;
} ConstStr;

#define CONST_STR(str)                                                         \
  (ConstStr) { .chars = #str, .length = sizeof(#str) - 1 }

#ifdef NAN_BOXING

typedef uint64_t Value;

static inline Value number_to_value(double num) {
  Value value;
  memcpy(&value, &num, sizeof(double));
  return value;
}

static inline double value_to_number(Value value) {
  double num;
  memcpy(&num, &value, sizeof(double));
  return num;
}

#define SIGN_BIT ((uint64_t)0x8000000000000000)
// TODO: do more reading on this
#define QNAN ((uint64_t)0x7ffc000000000000)

#define TAG_NIL 1
#define TAG_FALSE 2
#define TAG_TRUE 3

#define NUMBER_VAL(num) number_to_value(num)
#define OBJ_VAL(obj) (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))
#define NIL_VAL ((Value)(uint64_t)(QNAN | TAG_NIL))
#define FALSE_VAL ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define BOOL_VAL(b) ((b) ? TRUE_VAL : FALSE_VAL)

#define AS_NUMBER(value) value_to_number(value)
#define AS_BOOL(value) ((value) == TRUE_VAL)
#define AS_OBJ(value) ((Object *)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define IS_NIL(value) ((value) == NIL_VAL)
#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
#define IS_BOOL(value) ((value | 1) == TRUE_VAL)
#define IS_OBJ(value) (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#else

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

#define OBJ_TYPE_MASK 0x0F
#define OBJ_FLAG_MASK 0xF0

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value) ((value).type == VAL_OBJ)

#endif

#define OBJ_TYPE(value) (object_type(AS_OBJ(value)))
#define IS_FALSY(value) (value_is_falsy(value))

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
void value_print_indented(Value value, int indent);
void value_fprint(FILE *stream, Value value);
void value_fprintln(FILE *stream, Value value);
void value_fprint_indented(FILE *stream, Value value, int indent);
String *value_to_string(Value value);

bool value_equal(Value a, Value b);
bool value_is_falsy(Value value);
bool value_is_of_type(Value value, ValueType type);
ValueType value_type(Value value);
const ConstStr value_type_name(Value value);

#endif
