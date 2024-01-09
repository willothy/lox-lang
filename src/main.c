#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char *argv[]) {
	vm_init();

	chunk_t chunk;
	chunk_init(&chunk);

	for (int i = 0; i < 257; i++) {
		chunk_add_constant(&chunk, 9.0);
	}
	chunk_write_constant(&chunk, 1.2, 121);

	chunk_write(&chunk, OP_NEGATE, 121);

	chunk_write(&chunk, OP_RETURN, 123);

	disassemble_chunk(&chunk, "test chunk");

	vm_interpret(&chunk);

	vm_free();
	chunk_free(&chunk);

	return 0;
}
