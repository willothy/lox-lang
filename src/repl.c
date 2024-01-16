#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool repl_do_cmd(char *line, size_t length) {
	switch(line[1]) {
	case 'e':
		if (strncmp(line, ".exit", 5) == 0) {
			return true;
		}
		break;
	case 'l':
		if (strncmp(line, ".locals", 7) == 0) {
			printf("Locals:\n");
			// for (size_t i = locals_count-1; i > 0; i--) {
			// 	String *name = copy_string(locals[i].name.start, locals[i].name.length);
			// 	if (table_has_key(&seen, name)) {
			// 		continue;
			// 	}
			// 	printf("%.*s = ", (int)locals[i].name.length, locals[i].name.start);
			//
			// 	table_set( &seen, name, BOOL_VAL(true));
			// 	value_println(vm.running->current_frame->slots[i]);
			// }
			// seen.count = 0;
			// continue;
			return false;
		}
		break;
	case 's':
		if (strncmp(line, ".stack", 6) == 0) {
			printf("Stack:\n");
			for (Value *slot = vm.running->current_frame->slots; slot < vm.running->stack_top; slot++) {
				value_println(*slot);
			}
			return false;
		}
		break;
	default:
		break;
	}
	printf("Unknown command %s\n", line);
	return false;
}

ValueArray lines;
Table seen;
Compiler compiler;
Function *f = NULL;
char line[1024];

void repl() {
	value_array_init(&lines);
	table_init(&seen);

	bool compiler_has_init = false;

	for (;;) {
		printf("> ");

		if (!fgets(line, sizeof(line), stdin) || feof(stdin)) {
			printf("\n");
			break;
		}

		size_t len = strlen(line);
		line[len] = '\0';
		if (line[0] == '\n') continue;
		if (line[0] == '.') {
			if (repl_do_cmd(line, len)) {
				break;
			}
			continue;
		}

		String *str = copy_string(line, len);
		value_array_write(&lines, OBJ_VAL(str));

		if (!compiler_has_init) {
			compiler_init(&compiler, FN_TYPE_SCRIPT);

			f = compiler.function;
		}

		size_t code_size = f->chunk.count;

		compile_line(str->chars);

		if (!compiler_has_init) {
			compiler_has_init = true;
			vm_push(OBJ_VAL(f));
			Closure *closure = closure_new(f);
			vm_pop();
			vm_push(OBJ_VAL(closure));
			// chunk_write(&f->chunk, OP_CALL, 1);
			// chunk_write(&f->chunk, 0x00, 1);
			vm_call(closure, 0);
		}

		// printf("n bytes %zu\n", f->chunk.count);

		disassemble_chunk(&f->chunk, "script");
		vm_run(true);

		// f->chunk.code[0] = 0;
		// f->chunk.code[1] = 0;
		// f->chunk.code[2] = 0;
		// f->chunk.code[4] = 0;
		// vm.running->current_frame->ip = &f->chunk.code[f->chunk.count-2];//chunk_last_instruction_len(&f->chunk);
		// vm.running->current_frame->ip -= 2;//chunk_last_instruction_len(&f->chunk);
		// f->chunk.count -= 2;

		// Function *f = compile(str->chars);
		// last = this;
		// this = NULL;

		// if (f) {
		// vm_interpret(f);
		// };
	}
	end_compilation();
	table_free(&seen);
	value_array_free(&lines);
}

void repl_mark_roots() {
	for (size_t i = 0; i < lines.count; i++) {
		mark_value(lines.values[i]);
	}
	if (f) {
		mark_object((Object*)f);
	}
}
