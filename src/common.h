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

#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION

#define ALLOW_SHADOWING

#define UINT8_COUNT (UINT8_MAX + 1)
#define UINT32_COUNT (UINT32_MAX + 1)

#endif
