#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "memory.h"
#include "value.h"

typedef enum {
  OBJ_STRING = 0,
  OBJ_FUNCTION,
  OBJ_CLOSURE,
  OBJ_UPVALUE,
  OBJ_NATIVE,
} ObjectType;

struct Object {
  ObjectType type;
  // Whether the object's underlying memory is owned by the VM.
  // Heap allocations from Object are always owned by the VM and
  // should be GC'd, but data referenced internally by the object may
  // not be.
  //
  // Ex: strings and (in the future) ffi objects / C function pointers.
  //
  // Can have different behavior depending on the object type.
  //
  // For strings, this means that the string's `chars` array is owned by the
  // VM and should be freed when the wrapping ObjectString is freed.
  bool owned;
  struct Object *next;
};

// TODO: use [flexible array
// members](https://en.wikipedia.org/wiki/Flexible_array_member) to reduce
// indirection.
//
// TODO: implement utf-8 strings, with codepoint indexing. This will require
// some changes to the scanner and string literal parsing.
struct ObjectString {
  Object object;
  size_t length;
  uint32_t hash;
  char *chars;
};

static inline bool is_obj_type(Value value, ObjectType type) {
  return (IS_OBJ(value) && OBJ_TYPE(value) == type);
}

#define IS_STRING(value) is_obj_type(value, OBJ_STRING)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)
#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)
#define IS_UPVALUE(value) is_obj_type(value, OBJ_UPVALUE)

#define AS_STRING(value) ((ObjectString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjectString *)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjectFunction *)AS_OBJ(value))
#define AS_NATIVE(value) ((ObjectNative *)AS_OBJ(value))
#define AS_NATIVE_FN(value) (((ObjectNative *)AS_OBJ(value))->function)
#define AS_CLOSURE(value) ((ObjectClosure *)AS_OBJ(value))
#define AS_UPVALUE(value) ((ObjectUpvalue *)AS_OBJ(value))

typedef struct {
  Object object;
  Chunk chunk;
  ObjectString *name;
  uint8_t upvalue_count;
  uint8_t arity; // I don't think anyone will use more than 255 args ever.
} ObjectFunction;

typedef struct ObjectUpvalue {
  Object obj;
  Value *location;
  Value closed;
  struct ObjectUpvalue *next;
} ObjectUpvalue;

typedef struct {
  Object obj;
  // TODO: Only wrap functions that actually capture variables.
  ObjectFunction *function;
  ObjectUpvalue **upvalues;
  uint8_t upvalue_count;
} ObjectClosure;

typedef Value (*NativeFn)(uint8_t argc, Value *args);

// TODO: implement a way for native functions to throw errors.
typedef struct {
  Object obj;
  NativeFn function;
  uint8_t arity;
} ObjectNative;

ObjectString *copy_string(const char *start, size_t length);
ObjectString *take_string(char *chars, size_t length);
ObjectString *ref_string(char *chars, size_t length);

ObjectUpvalue *upvalue_new(Value *slot);

ObjectClosure *closure_new(ObjectFunction *function);
ObjectFunction *function_new();
void function_print(ObjectFunction *function);

ObjectNative *native_new(NativeFn function, uint8_t arity);

const char *object_type_name(ObjectType type);
void object_print(Value obj);

#endif
