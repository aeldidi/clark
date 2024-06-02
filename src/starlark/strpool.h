#ifndef UTIL_STRPOOL_H
#define UTIL_STRPOOL_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "starlark/common.h"

// Initializes the strpool with enough space for 100 handles.
// Returns false if we couldn't allocate enough memory.
bool strpool_init(struct starlark_Strpool *s);

// Returns either a positive handle, or a negative error code.
int64_t strpool_add(struct starlark_Strpool *s, const size_t len,
		    const char *str);

// Returns the string associated with the handle, or NULL if the handle is
// invalid.
const char *strpool_get(struct starlark_Strpool *s, const int64_t handle);

void strpool_finish(struct starlark_Strpool *s);

#endif // UTIL_STRPOOL_H
