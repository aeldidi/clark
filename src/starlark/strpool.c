#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "starlark/strpool.h"
#include "util/fnv-1a.h"
#include "util/common.h"
#include "util/panic.h"

static bool strpool_ensure(struct starlark_Strpool *s, const size_t handles,
			   const size_t bytes)
{
	assert(s != NULL);

	if (bytes < s->buffer.cap) {
		return true;
	}

	size_t new_cap = MAX(handles, s->table.cap);

	size_t hashes_len = new_cap * sizeof(s->table.hashes[0]);
	size_t handles_len = new_cap * sizeof(s->table.handles[0]);
	size_t buffer_len = bytes * sizeof(s->buffer.ptr[0]);

	if (new_cap > INT64_MAX || buffer_len > INT64_MAX) {
		return false;
	}

	uint8_t *memory =
		realloc(s->table.hashes, hashes_len + handles_len + buffer_len);
	if (memory == NULL) {
		return false;
	}

	s->table.hashes = (void *)memory;
	s->table.handles = (void *)(memory + hashes_len);
	s->buffer.ptr = (void *)(memory + hashes_len + handles_len);

	s->table.cap = new_cap;
	s->buffer.cap = bytes;

	return true;
}

bool strpool_init(struct starlark_Strpool *s)
{
	assert(s != NULL);

	size_t hashes_len = 100 * sizeof(s->table.hashes[0]);
	size_t handles_len = 100 * sizeof(s->table.handles[0]);
	size_t buffer_len = 100 * sizeof(s->buffer.ptr[0]);

	uint8_t *memory = malloc(hashes_len + handles_len + buffer_len);
	if (memory == NULL) {
		return false;
	}

	s->table.hashes = (void *)memory;
	s->table.handles = (void *)(memory + hashes_len);
	s->buffer.ptr = (void *)(memory + hashes_len + handles_len);

	s->table.len = 0;
	s->table.cap = 100;
	s->buffer.len = 1;
	s->buffer.cap = 100;
	s->buffer.ptr[0] = '\0';

	return true;
}

int64_t strpool_add(struct starlark_Strpool *s, const size_t len,
		    const char *str)
{
	assert(s != NULL);
	int64_t hash = 0;
	uint64_t tmp = fnv_1a_str(str);
	memcpy(&hash, &tmp, sizeof(int64_t));

	// printf("adding '%.*s' to the pool\n", (int)len, str);

	for (size_t i = 0; i < s->table.len; i += 1) {
		if (hash == s->table.hashes[i]) {
			return s->table.handles[i];
		}
	}

	if (s->buffer.len + len < s->buffer.len) {
		return -1;
	}

	if (!strpool_ensure(s, s->table.len + 1, s->buffer.len + len + 1)) {
		return -1;
	}

	int64_t result = s->buffer.len;

	s->table.hashes[s->table.len] = hash;
	s->table.handles[s->table.len] = result;
	memcpy(&s->buffer.ptr[s->buffer.len], str, len + 1);
	s->buffer.len += len + 1;
	s->table.len += 1;

	// printf("returning handle %" PRId64 "\n", result);

	return result;
}

static bool is_in_strpool(struct starlark_Strpool *s, const int64_t handle)
{
	for (size_t i = 0; i < s->table.len; i += 1) {
		if (s->table.handles[i] != handle) {
			continue;
		}

		return true;
	}

	return false;
}

const char *strpool_get(struct starlark_Strpool *s, const int64_t handle)
{
	assert(s != NULL);
	assert(handle >= 0);

	// printf("retrieving handle %" PRId64 "\n", handle);

	if (handle == 0) {
		return &s->buffer.ptr[0];
	}

	if (!is_in_strpool(s, handle)) {
		panic("handle %" PRId64 " is not in strpool", handle);
	}

	return &s->buffer.ptr[handle];
}

void strpool_finish(struct starlark_Strpool *s)
{
	if (s == NULL) {
		return;
	}

	free(s->table.hashes);
}
