#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "memory.h"
#include "table.h"
#include "value.h"

#define FRAMES_INITIAL 64
#define STACK_INITIAL (FRAMES_INITIAL * UINT8_COUNT)

#define FRAMES_MAX (UINT8_COUNT)
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct Object {
  // Header "contains" the following fields:
  // struct Object *next;
  // bool marked;
  // // Whether the object's underlying memory is owned by the VM.
  // // Heap allocations from Object are always owned by the VM and
  // // should be GC'd, but data referenced internally by the object may
  // // not be.
  // //
  // // Ex: strings and (in the future) ffi objects / C function pointers.
  // //
  // // Can have different behavior depending on the object type.
  // //
  // // For strings, this means that the string's `chars` array is owned by the
  // // VM and should be freed when the wrapping ObjectString is freed.
  // bool owned;
  // ObjectType type;
  //
  // 01111111 11010110 01001111 01010000 00000000 01100000 00000000 00000000
  // Bit position:
  // 44444444 33333333 33222222 22221111 11111100 00000000 66665555 55555544
  // 76543210 98765432 10987654 32109876 54321098 76543210 32109876 54321098
  //
  // Bits needed for pointer:
  // ........ ........ |------- -------- -------- ------- --------- ----|...
  //
  // Packing everything in:
  // NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN ......OM .....TTT
  //
  // T = type enum,
  // M = mark bit,
  // O = owned bit,
  // N = next pointer.
  uint64_t header;
} Object;

static inline ObjectType object_type(Object *obj) {
  return (ObjectType)(obj->header & 0xff);
}

static inline void object_set_type(Object *obj, ObjectType type) {
  obj->header = (obj->header & 0xffffffffffffff00) | (uint64_t)type;
}

static inline bool object_is_type(Object *obj, ObjectType type) {
  return object_type(obj) == type;
}

static inline void object_set_next(Object *obj, Object *next) {
  obj->header = (obj->header & 0x00000000000000ff) | ((uint64_t)next << 16);
}

static inline bool object_is_marked(Object *obj) {
  return (bool)((obj->header >> 8) & 0x01);
}

static inline void object_mark(Object *obj) {
  // STOP PUTTING THIS ON ONE LINE, UNCRUSTIFY
  obj->header |= (uint64_t)1 << 8;
}

static inline void object_unmark(Object *obj) {
  obj->header &= ~((uint64_t)1 << 8);
}

static inline bool object_is_owned(Object *obj) {
  return (bool)((obj->header >> 9) & 0x01);
}

static inline void object_set_owned(Object *obj) {
  obj->header |= (uint64_t)1 << 9;
}

static inline void object_set_non_owned(Object *obj) {
  obj->header &= ~((uint64_t)1 << 9);
}

static inline Object *object_next(Object *obj) {
  return (Object *)(obj->header >> 16);
}

// TODO: use [flexible array
// members](https://en.wikipedia.org/wiki/Flexible_array_member) to reduce
// indirection.
//
// TODO: implement utf-8 strings, with codepoint indexing. This will require
// some changes to the scanner and string literal parsing.
struct String {
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
#define IS_LIST(value) is_obj_type(value, OBJ_LIST)
#define IS_DICT(value) is_obj_type(value, OBJ_DICT)
#define IS_COROUTINE(value) is_obj_type(value, OBJ_COROUTINE)

#define AS_STRING(value) ((String *)AS_OBJ(value))
#define AS_CSTRING(value) (((String *)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((Function *)AS_OBJ(value))
#define AS_NATIVE(value) ((NativeFunction *)AS_OBJ(value))
#define AS_NATIVE_FN(value) (((NativeFunction *)AS_OBJ(value))->function)
#define AS_CLOSURE(value) ((Closure *)AS_OBJ(value))
#define AS_UPVALUE(value) ((Upvalue *)AS_OBJ(value))
#define AS_LIST(value) ((List *)AS_OBJ(value))
#define AS_DICT(value) ((Dictionary *)AS_OBJ(value))
#define AS_COROUTINE(value) ((Coroutine *)AS_OBJ(value))

typedef struct {
  Object object;
  Chunk chunk;
  String *name;
  uint8_t upvalue_count;
  uint8_t arity; // I don't think anyone will use more than 255 args ever.
  // TODO: do I want to support multiple return values an varargs?
} Function;

typedef struct Upvalue {
  Object obj;
  Value *location;
  Value closed;
  struct Upvalue *next;
} Upvalue;

typedef struct {
  Object obj;
  // TODO: Only wrap functions that actually capture variables.
  Function *function;
  Upvalue **upvalues;
  uint8_t upvalue_count;
} Closure;

typedef Value (*NativeFnPtr)(uint8_t argc, Value *args);

// TODO: implement a way for native functions to throw errors.
typedef struct {
  Object obj;
  NativeFnPtr function;
  uint8_t arity;
} NativeFunction;

typedef struct {
  Object obj;
  ValueArray values;
} List;

typedef struct {
  Object obj;
  Table table;
} Dictionary;

typedef enum {
  // The coroutine has been created but has not been started yet.
  COROUTINE_READY,
  // The coroutine has yielded and is waiting to be resumed.
  COROUTINE_PAUSED,
  // The coroutine is currently running, and is either the current coroutine
  // or one of its ancestors.
  COROUTINE_RUNNING,
  // The coroutine has finished executing.
  COROUTINE_COMPLETE,
  // The coroutine has thrown an error.
  COROUTINE_ERROR,
} CoroutineState;

typedef struct {
  Closure *closure;
  uint8_t *ip;
  Value *slots;
} CallFrame;

typedef struct Coroutine {
  Object obj;
  struct Coroutine *parent;

  // This exists so that we can iterate over all coroutines during
  // garbage collection without needing do maintain a list of valid
  // pointers to all coroutines. The main coroutine should *always*
  // be the head of the list.
  // struct Coroutine *next;

  CallFrame *frames;
  // We *must* ensure that this never exceeds some maximum
  // to avoid memory leaks on infinite recursion.
  size_t frame_capacity;
  size_t frame_count;
  // The current frame is frames[frame_count - 1].
  CallFrame *current_frame;

  Value *stack;
  // We *must* ensure that this never exceeds some maximum
  // to avoid memory leaks on infinite recursion.
  size_t stack_size;
  Value *stack_top;

  CoroutineState state;
} Coroutine;

Coroutine *coroutine_new(Closure *closure);
void coroutine_reset(Coroutine *coroutine);
void coroutine_free(Coroutine *coroutine);
void coroutine_push(Coroutine *coroutine, Value value);
Value coroutine_pop(Coroutine *coroutine);
Value coroutine_peek(Coroutine *coroutine, size_t distance);

String *copy_string(const char *start, size_t length);
String *take_string(char *chars, size_t length);
String *ref_string(char *chars, size_t length);
String *const_string(const char *chars, size_t length);
void string_print(String *string);

List *list_new();
void list_set(List *list, size_t index, Value value);
Value list_remove(List *list, size_t index);
Value list_pop(List *list);
void list_push(List *list, Value value);
Value list_get(List *list, size_t index);
size_t list_length(List *list);

Dictionary *dict_new();
void dict_set(Dictionary *dict, String *key, Value value);
void dict_clear(Dictionary *dict);
Value dict_remove(Dictionary *dict, String *key);
Value dict_get(Dictionary *dict, String *key);

// void dict_keys(ObjectDict *dict, ObjectList *list);
// void dict_values(ObjectDict *dict, ObjectList *list);

Upvalue *upvalue_new(Value *slot);

Closure *closure_new(Function *function);
Function *function_new();
void function_print(Function *function);

NativeFunction *native_new(NativeFnPtr function, uint8_t arity);

const char *object_type_name(ObjectType type);
void object_print(Value obj);
void object_print_indented(Value obj, int indent);

#endif
