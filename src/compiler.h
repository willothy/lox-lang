#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
#include "object.h"

Function *compile(char *source);
void compiler_mark_roots();

#endif
