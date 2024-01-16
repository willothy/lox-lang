#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
#include "object.h"
#include "scanner.h"

typedef struct {
  Token name;
  uint32_t depth;
  bool is_captured;
} Local;

typedef struct {
  uint32_t index;
  bool is_local;
} UpvalueMeta;

typedef enum {
  FN_TYPE_NAMED,
  FN_TYPE_ANONYMOUS,
  FN_TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
  struct Compiler *enclosing;

  Function *function;
  FunctionType type;

  Local locals[UINT8_COUNT];
  uint32_t local_count;

  UpvalueMeta upvalues[UINT8_COUNT];
  uint32_t upvalue_count;

  uint32_t scope_depth;
} Compiler;

Function *compile(char *source);
void compile_line(char *source);
void compiler_init(Compiler *compiler, FunctionType type);
void scanner_init(char *source);
Function *end_compilation();
void compiler_mark_roots();
void declaration();
void emit_byte(uint8_t byte);

#endif
