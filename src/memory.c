#include <stdlib.h>

#include "memory.h"
#include "vm.h"

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

static void free_object(object_t *obj) {
	switch (obj->type) {
	case OBJ_STRING: {
		object_string_t *str = (object_string_t *)obj;
		if (obj->owned) {
			FREE_ARRAY(char, str->chars, str->length + 1);
		}
		FREE(object_string_t, obj);
		break;
	}
	}
}

void free_objects() {
	object_t *obj = vm.objects;
	while (obj) {
		object_t *next = obj->next;
		free_object(obj);
		obj = next;
	}
	vm.objects = NULL;
}
