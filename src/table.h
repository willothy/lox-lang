#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
  ObjectString *key;
  Value value;
} Entry;

typedef struct {
  size_t count;
  size_t capacity;
  Entry *entries;
} Table;

void table_init(Table *table);
void table_free(Table *table);
void table_add_all(Table *from, Table *to);
bool table_set(Table *table, ObjectString *key, Value value);
bool table_get(Table *table, ObjectString *key, Value *value);
bool table_delete(Table *table, ObjectString *key);
ObjectString *table_find_string(Table *table, const char *chars,
                                   size_t length, uint32_t hash);

void table_print(Table *table, char *name);

#endif
