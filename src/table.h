#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
  object_string_t *key;
  value_t value;
} entry_t;

typedef struct {
  size_t count;
  size_t capacity;
  entry_t *entries;
} table_t;

void table_init(table_t *table);
void table_free(table_t *table);

#endif
