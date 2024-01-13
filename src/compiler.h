#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
#include "object.h"

ObjectFunction *compile(char *source);
void compiler_mark_roots();

#endif
