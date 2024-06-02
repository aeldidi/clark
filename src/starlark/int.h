#ifndef STARLARK_INT_H
#define STARLARK_INT_H
#include <stdio.h>
#include <stdint.h>

#include "starlark/common.h"

// This only exists to allow switching out the bigint library used without
// needing too many code changes.

#define INT60_MAX INT64_C(576460752303423487)
#define INT60_MIN INT64_C(-576460752303423488)

struct starlark_Int *Int_create();
struct starlark_Int *Int_create_digits(const size_t digits);
void Int_destroy(const struct starlark_Int *i);

void Int_set_u32(struct starlark_Int *a, const uint32_t b);

// Computes c = a + b
// Returns nonzero on failure.
int Int_add_u32(const struct starlark_Int *a, const uint32_t b,
		struct starlark_Int *c);

// Computes c = a + b
// Returns nonzero on failure.
int Int_add(const struct starlark_Int *a, const struct starlark_Int *b,
	    struct starlark_Int *c);

// Computes c = a ** b.
// Returns nonzero on failure.
int Int_pow_u32(const struct starlark_Int *a, const uint32_t b,
		struct starlark_Int *c);

// Computes c = a * b.
// Returns nonzero on failure.
int Int_mul(const struct starlark_Int *a, const struct starlark_Int *b,
	    struct starlark_Int *c);

// Returns NULL on failure.
char *Int_to_str(const struct starlark_Int *i, const int base);

// Returns NULL on failure.
struct starlark_Int *Int_from_str(const char *str, const int base);

#endif // STARLARK_INT_H
