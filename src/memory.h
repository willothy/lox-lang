#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "value.h"

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, old_count, new_count)                        \
  (type *)reallocate(pointer, sizeof(type) * (old_count),                      \
                     sizeof(type) * (new_count))

#define FREE_ARRAY(type, pointer, count)                                       \
  reallocate(pointer, sizeof(type) * (count), 0)

void *reallocate(void *ptr, size_t old_size, size_t new_size);
void mark_value(Value value);
void mark_object(Object *object);
void collect_garbage();
void free_objects();

#define ALLOCATE(type, count) (type *)reallocate(NULL, 0, count * sizeof(type))

#endif
