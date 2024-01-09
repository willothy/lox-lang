#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

void table_init(table_t *table) {
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void table_free(table_t *table) {
	FREE_ARRAY(entry_t, table->entries, table->capacity);
	table_init(table);
}
