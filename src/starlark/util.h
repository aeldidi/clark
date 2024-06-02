#ifndef STARLARK_UTIL_H
#define STARLARK_UTIL_H
#include <stdbool.h>
#include <stdint.h>

#include "starlark/common.h"

bool err_append(struct starlark_Context *in, const struct starlark_Error err);

bool starlark_isspace(const uint32_t c);
bool starlark_isbytes(const uint32_t c);
bool starlark_isalpha(const uint32_t c);
bool starlark_isdigit(const uint32_t c);
bool starlark_isident(const uint32_t c);

#endif // STARLARK_UTIL_H
