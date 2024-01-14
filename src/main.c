#include "compiler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <io.h>
#define R_OK 4
#define access _access
#else
#include <unistd.h>
#endif

#include "vm.h"

static Function *try_compile(char *src) {
	Function *function = compile(src);
	if (!function) {
		exit(65);
	}
	return function;
}

static void repl() {
	char line[1024];
	for ( ; ; ) {
		printf("> ");

		if (!fgets(line, sizeof(line), stdin) || feof(stdin)) {
			printf("\n");
			break;
		}

		Function *f = compile(line);

		if (f) vm_interpret(f);
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

static void run_file(const char *path, const char *output_path) {
	char *src = read_file(path);

	Function *function = try_compile(src);

	if (output_path) {
		// FILE *file = fopen(output_path, "wb");
		// if (!file) {
		// 	printf("Could not open file %s\n", output_path);
		// 	exit(74);
		// }
		// char *magic = "CLOX";
		// Chunk chunk = function->chunk;
		// TODO: figure out how to write const strings to/from bytecode file,
		// transition from constants array to static segment in code.
		// size_t buf_size =
		// 	4 // magic
		// 	+ 1 // code size
		// 	+ 1 // debug info size
		// 	+ 1 // constants size
		// 	+ chunk.count // code
		// 	+ (chunk.lines.count * sizeof(Line)) // debug info
		// 	+ (chunk.constants.count * sizeof(Value)) // constants
		// 	;
		// fclose(file);
	}

	InterpretResult res = vm_interpret(function);
	free(src);

	if (res == INTERPRET_COMPILE_ERROR) exit(65);
	if (res == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char *argv[]) {
	char * err = vm_init();
	if (err) {
		printf("%s\n", err);
		abort();
	}

	const char *input = NULL;
	const char *output = NULL;
	bool error = false;
	switch(argc) {
	case 1:
		repl();
		break;
	default:
		for (int i = 1; i < argc; i++) {
			if (strncmp(argv[i], "-o", 2) == 0 || strncmp(argv[i], "--output", 8) == 0) {
				output = argv[++i];
				if (output == NULL) {
					error = true;
					printf("Missing output file name\n");
					break;
				}
			} else {
				if (access(argv[i], R_OK) == 0) {
					input = argv[i];
				} else {
					printf("Unexpected argument or nonexistent file %s\n", argv[i]);
					error = true;
					break;
				}
			}
		}
		if (error || input == NULL) {
			if (input == NULL) {
				printf("Missing input file name\n");
			}
			printf(
				"Usage: clox [path]\n"
				"\n"
				"  -o --output <file> Output bytecode\n"
				);
			break;
		} else {
			run_file(input, output);
		}
		break;
	}

	vm_free();

	return 0;
}
