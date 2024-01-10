#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "debug.h"
#include "vm.h"

static void repl() {
	char line[1024];
	for ( ; ; ) {
		printf("> ");

		if (!fgets(line, sizeof(line), stdin) || feof(stdin)) {
			printf("\n");
			break;
		}

		vm_interpret(line);
		table_print(&vm.globals, "globals");
	}
}

static char *read_file(const char *path) {
	FILE *file = fopen(path, "rb");
	if (!file) {
		printf("Could not open file %s\n", path);
		exit(74);
	}

	fseek(file, 0, SEEK_END);
	size_t file_size = ftell(file);
	rewind(file);

	char *buffer = (char*)malloc(file_size + 1);
	if (!buffer) {
		printf("Could not allocate memory for reading file \"%s\"\n", path);
		exit(74);
	}
	size_t bytes = fread(buffer, sizeof(char), file_size, file);
	buffer[bytes] = '\0';

	fclose(file);

	return buffer;
}

static void run_file(const char *path) {
	char *src = read_file(path);
	interpret_result_t res = vm_interpret(src);
	free (src);

	if (res == INTERPRET_COMPILE_ERROR) exit(65);
	if (res == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char *argv[]) {
	char * err = vm_init();
	if (err) {
		printf("%s\n", err);
		abort();
	}

	switch(argc) {
	case 1:
		repl();
		break;
	case 2:
		run_file(argv[1]);
		break;
	default:
		printf("Usage: clox [path]\n");
		break;
	}

	vm_free();

	return 0;
}
