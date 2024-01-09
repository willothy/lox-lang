#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

typedef enum {
  OBJ_STRING,
} object_type_t;

struct object_t {
  object_type_t type;
  // Whether the object's underlying memory is owned by the VM.
  // Heap allocations from object_t are always owned by the VM and
  // should be GC'd, but data referenced internally by the object may
  // not be.
  //
  // Ex: strings and (in the future) ffi objects / C function pointers.
  //
  // Can have different behavior depending on the object type.
  //
  // For strings, this means that the string's `chars` array is owned by the
  // VM and should be freed when the wrapping object_string_t is freed.
  bool owned;
  struct object_t *next;
};

// TODO: use [flexible array
// members](https://en.wikipedia.org/wiki/Flexible_array_member) to reduce
// indirection.
//
// TODO: implement utf-8 strings, with codepoint indexing. This will require
// some changes to the scanner and string literal parsing.
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
object_string_t *take_string(char *chars, size_t length);
object_string_t *ref_string(char *chars, size_t length);

void object_print(value_t obj);

#endif
