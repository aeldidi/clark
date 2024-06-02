#ifndef STARLARK_STARLARK_H
#define STARLARK_STARLARK_H

#include <stddef.h>
#include <stdint.h>

#include "starlark/common.h"

STARLARK_PUBLIC
struct starlark_Context starlark_init();

STARLARK_PUBLIC
void starlark_finish(struct starlark_Context *ctx);

// Executes src in the starlark context given.
STARLARK_PUBLIC
int starlark_exec(struct starlark_Context *ctx, const size_t src_len,
		  const uint8_t *src);

// Executes the contents of f in the starlark context ctx.
STARLARK_PUBLIC
int starlark_execfile(struct starlark_Context *ctx, FILE *f);

// Executes src in the starlark context given.
STARLARK_PUBLIC
int64_t starlark_thread_exec(struct starlark_Context *ctx, const size_t src_len,
			     const uint8_t *src);

// Executes the contents of f in the starlark context ctx.
STARLARK_PUBLIC
int64_t starlark_thread_execfile(struct starlark_Context *ctx, FILE *f);

STARLARK_PUBLIC
int starlark_wait(struct starlark_Context *ctx);

STARLARK_PUBLIC
int starlark_wait_thread(struct starlark_Context *ctx, const int64_t thread);

#endif // STARLARK_STARLARK_H
