#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
#include "object.h"

ObjectFunction *compile(const char *source);

#endif
