#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ASSERT(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      printf("Assertion failed: %s\n", #expr);                                 \
      exit(1);                                                                 \
    }                                                                          \
  } while (false)

#endif
