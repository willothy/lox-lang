#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
  String *key;
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
bool table_set(Table *table, String *key, Value value);
bool table_get(Table *table, String *key, Value *value);
bool table_delete(Table *table, String *key);
bool table_get_and_delete(Table *table, String *key, Value *value);
void table_remove_white(Table *table);
String *table_find_string(Table *table, const char *chars, size_t length,
                                uint32_t hash);

void table_print(Table *table, char *name);

void table_mark(Table *table);

#endif
