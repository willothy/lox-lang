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
void table_add_all(table_t *from, table_t *to);
bool table_set(table_t *table, object_string_t *key, value_t value);
bool table_get(table_t *table, object_string_t *key, value_t *value);
bool table_delete(table_t *table, object_string_t *key);
object_string_t *table_find_string(table_t *table, const char *chars,
                                   size_t length, uint32_t hash);

void table_print(table_t *table, char *name);

#endif
