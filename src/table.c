#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "table.h"
#include "value.h"
#include "object.h"

#define TABLE_MAX_LOAD 0.75

void table_init(table_t *table) {
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void table_free(table_t *table) {
	FREE_ARRAY(entry_t, table->entries, table->capacity);
	table_init(table);
}

static entry_t *table_find_entry(entry_t *entries, size_t capacity, object_string_t *key) {
	uint32_t index = key->hash % capacity;
	entry_t *tombstone = NULL;

	for (;;) {
		entry_t *entry = &entries[index];
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

static void adjust_capacity(table_t *table, size_t capacity) {
	entry_t *entries = ALLOCATE(entry_t, capacity);
	for (size_t i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}

	table->count = 0;

	// TODO: Research how to optimize this
	for (size_t i = 0; i < table->capacity; i++) {
		entry_t *entry = &table->entries[i];
		if (entry->key == NULL) {
			continue;
		}

		entry_t *dest = table_find_entry(entries, capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		table->count++;
	}

	// free the old entries array
	FREE_ARRAY(entry_t, table->entries, table->capacity);

	table->entries = entries;
	table->capacity = capacity;
}

bool table_set(table_t *table, object_string_t *key, value_t value) {
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		size_t capacity = GROW_CAPACITY(table->capacity);
		adjust_capacity(table, capacity);
	}

	entry_t *entry = table_find_entry(table->entries, table->capacity, key);
	bool is_new = entry->key == NULL;
	if (is_new) {
		table->count++;
	}

	entry->key = key;
	entry->value = value;
	return is_new;
}

void table_add_all(table_t *from, table_t *to) {
	for (size_t i = 0; i < from->capacity; i++) {
		entry_t *entry = &from->entries[i];
		if (entry->key != NULL) {
			table_set(to, entry->key, entry->value);
		}
	}
}

bool table_get(table_t *table, object_string_t *key, value_t *value) {
	if (table->count == 0) {
		return false;
	}
	entry_t *entry = table_find_entry(table->entries, table->capacity, key);
	if (entry->key == NULL) {
		return false;
	}
	*value = entry->value;
	return true;
}

bool table_delete(table_t *table, object_string_t *key) {
	if (table->count == 0) {
		return false;
	}
	entry_t *entry = table_find_entry(table->entries, table->capacity, key);
	if (entry->key == NULL) {
		return false;
	}
	entry->key = NULL;
	entry->value = BOOL_VAL(true);
	return true;
}
