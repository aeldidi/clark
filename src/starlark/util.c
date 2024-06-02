#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdalign.h>

#include "starlark/common.h"
#include "starlark/strpool.h"
#include "util/common.h"
#include "utf8/utf8.h"

bool err_append(struct starlark_Context *in, const struct starlark_Error err)
{
	assert(in != NULL);
	if (in->errs_len == UINT16_MAX) {
		return true;
	}

	if (in->errs_len + 1 >= in->errs_cap) {
		size_t cap = (in->errs_cap + 16) * 1.5;
		if (cap >= UINT16_MAX) {
			cap = UINT16_MAX;
		}
		size_t codes_len = cap * sizeof(in->errs.codes[0]);
		size_t starts_len = cap * sizeof(in->errs.starts[0]);
		size_t msgs_len = cap * sizeof(in->errs.msgs[0]);
		uint8_t *ptr =
			realloc(in->errs.codes,
				codes_len + starts_len + msgs_len +
					alignof(enum starlark_ErrorCode) +
					alignof(size_t) + alignof(int64_t));
		if (ptr == NULL) {
			return false;
		}

		in->errs.codes = (void *)ptr;
		in->errs.starts =
			(void *)ALIGN_UP(ptr + codes_len, alignof(size_t));
		in->errs.msgs = (void *)ALIGN_UP(ptr + codes_len + starts_len,
						 alignof(int64_t));
		in->errs_cap = cap;
	}

	int64_t handle = 0;
	if (err.msg != NULL) {
		handle = strpool_add(&in->strpool, strlen(err.msg), err.msg);
		if (handle < 0) {
			return false;
		}

		free((char *)err.msg);
	}

	// printf("len = %zu\n", in->errs_len);
	fflush(stdout);

	in->errs.codes[in->errs_len] = err.code;
	in->errs.starts[in->errs_len] = err.start;
	in->errs.msgs[in->errs_len] = handle;
	in->errs_len += 1;
	return true;
}

bool starlark_isspace(const uint32_t c)
{
	return c == (uint8_t)u8" "[0] || c == (uint8_t)u8"\t"[0] ||
	       c == (uint8_t)u8"\r"[0];
}

bool starlark_isbytes(const uint32_t c)
{
	return c <= 0x7f;
}

bool starlark_isalpha(const uint32_t c)
{
	return (c >= (uint8_t)u8"a"[0] && c <= (uint8_t)u8"z"[0]) ||
	       (c >= (uint8_t)u8"A"[0] && c <= (uint8_t)u8"Z"[0]);
}

bool starlark_isdigit(const uint32_t c)
{
	return c >= (uint8_t)u8"0"[0] && c <= (uint8_t)u8"9"[0];
}

bool starlark_isident(const uint32_t c)
{
	return utf8_codepoint_valid(c) &&
	       (c == (uint8_t)u8"_"[0] || starlark_isdigit(c) ||
		starlark_isalpha(c));
}
