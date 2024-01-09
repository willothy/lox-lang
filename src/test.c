#include <stdio.h>
#include <stdlib.h>

int tests_run = 0;

typedef struct  {
	char *failure; // NULL if no failure
	const char *name;
} result_t;

result_t *result_new(char *name) {
	result_t *result = malloc(sizeof(result_t));
	result->name = name;
	result->failure = NULL;
	return result;
}

void result_fail(result_t *result, char *message) {
	result->failure = message;
}

void result_print(result_t *result) {
	if (result->failure) {
		fprintf(stderr, "Test %s failed: %s\n", result->name, result->failure);
	} else {
		fprintf(stderr, "Test %s passed\n", result->name);
	}
}

void result_free(result_t *result) {
	if (result->failure) {
		free(result->failure);
	}
	free(result);
}

typedef result_t (*test_func_t)();

#define ASSERT(condition) do { \
		if (!(condition)) { \
			fprintf(stderr, "Assertion failed: %s\n", #condition); \
			abort(); \
		} \
} while (0)

#define TEST(name, fn) do { \
		result_t *result = result_new(name); \
		result_t result_ = fn(); \
		if (result_.failure) { \
			result_fail(result, result_.failure); \
		} \
		result_print(result); \
		result_free(result); \
		tests_run++; \
} while (0)

result_t test_chunk_write_constant() {
	return (result_t) { .name = "test_chunk_write_constant" };
}

int main() {
	// mu_run_test(test_chunk_write_constant);
	TEST("aouhfoiw", test_chunk_write_constant);

	return 0;
}
