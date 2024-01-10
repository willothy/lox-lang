#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "table.h"
#include "object.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void table_init(Table *table) {
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void table_free(Table *table) {
	FREE_ARRAY(Entry, table->entries, table->capacity);
	table_init(table);
}

static Entry *table_find_entry(Entry *entries, size_t capacity, ObjectString *key) {
	uint32_t index = key->hash % capacity;
	Entry *tombstone = NULL;

	for (;;) {
		Entry *entry = &entries[index];
		if (entry->key == NULL) {
			if (IS_NIL(entry->value)) {
				if (tombstone == NULL) {
					return entry;
				} else {
					return tombstone;
				}
			} else {
				if (tombstone == NULL) {
					tombstone = entry;
				}
			}
		} else if (entry->key == key) {
			return entry;
		}
		index = (index + 1) % capacity;
	}
}

static void adjust_capacity(Table *table, size_t capacity) {
	Entry *entries = ALLOCATE(Entry, capacity);
	for (size_t i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}

	table->count = 0;

	// TODO: Research how to optimize this
	for (size_t i = 0; i < table->capacity; i++) {
		Entry *entry = &table->entries[i];
		if (entry->key == NULL) {
			continue;
		}

		Entry *dest = table_find_entry(entries, capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		table->count++;
	}

	// free the old entries array
	FREE_ARRAY(Entry, table->entries, table->capacity);

	table->entries = entries;
	table->capacity = capacity;
}

bool table_set(Table *table, ObjectString *key, Value value) {
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		size_t capacity = GROW_CAPACITY(table->capacity);
		adjust_capacity(table, capacity);
	}

	Entry *entry = table_find_entry(table->entries, table->capacity, key);
	bool is_new = entry->key == NULL;
	if (is_new) {
		table->count++;
	}

	entry->key = key;
	entry->value = value;
	return is_new;
}

void table_add_all(Table *from, Table *to) {
	for (size_t i = 0; i < from->capacity; i++) {
		Entry *entry = &from->entries[i];
		if (entry->key != NULL) {
			table_set(to, entry->key, entry->value);
		}
	}
}

bool table_get(Table *table, ObjectString *key, Value *value) {
	if (table->count == 0) {
		return false;
	}
	Entry *entry = table_find_entry(table->entries, table->capacity, key);
	if (entry->key == NULL) {
		return false;
	}
	*value = entry->value;
	return true;
}

bool table_delete(Table *table, ObjectString *key) {
	if (table->count == 0) {
		return false;
	}
	Entry *entry = table_find_entry(table->entries, table->capacity, key);
	if (entry->key == NULL) {
		return false;
	}
	entry->key = NULL;
	entry->value = BOOL_VAL(true);
	return true;
}

ObjectString* table_find_string(Table *table, const char *chars, size_t length, uint32_t hash) {
	if (table->count == 0) {
		return NULL;
	}
	uint32_t index = hash % table->capacity;
	for (;;) {
		Entry *entry = &table->entries[index];
		if (entry->key == NULL) {
			// stop if we find an empty non-tombstone entry
			if (IS_NIL(entry->value)) {
				return NULL;
			};
		} else if (entry->key->length == length &&
		           entry->key->hash == hash &&
		           memcmp(entry->key->chars, chars, length) == 0) {
			// found the string
			return entry->key;
		}
		index = (index + 1) % table->capacity;
	}
}

void table_print(Table *table, char *name) {
	if (name) {
		printf("%s: {", name);
	} else {
		printf("{");
	}
	if (table->count == 0) {
		printf("}\n");
		return;
	} else {
		printf("\n");
	}
	for (size_t i = 0; i < table->capacity; i++) {
		Entry *entry = &table->entries[i];
		if (entry->key != NULL) {
			printf("  ");
			object_print(OBJ_VAL(entry->key));
			printf(": ");
			value_println(entry->value);
		}
	}
	printf("}\n");
}
