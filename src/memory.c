#include <stdlib.h>

#include "memory.h"


void *reallocate(void *ptr, size_t old_size, size_t new_size) {
	if (new_size == 0) {
		free(ptr);
		return NULL;
	}
	void *rv = realloc(ptr, new_size);
	if (rv == NULL) {
		exit(1);
	}
	return rv;
}
