#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

typedef enum {
  OBJ_STRING,
} object_type_t;

struct object_t {
  object_type_t type;
};

struct object_string_t {
  object_t object;
  size_t length;
  char *chars;
};

static inline bool is_obj_type(value_t value, object_type_t type) {
  return (IS_OBJ(value) && OBJ_TYPE(value) == type);
}

#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_STRING(value) ((object_string_t *)AS_OBJ(value))
#define AS_CSTRING(value) (((object_string_t *)AS_OBJ(value))->chars)

object_string_t *copy_string(const char *start, size_t length);

void object_print(value_t obj);

#endif
