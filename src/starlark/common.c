#include <inttypes.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "starlark/common.h"
#include "starlark/parse.h"
#include "starlark/int.h"
#include "starlark/strpool.h"
#include "util/panic.h"
#include "util/lineno.h"

// TODO: i18n
// These are the 'reason' part of an error message. For example, given the
// error:
//
// file:1:1: invalid digit in binary number: '0'
//
// The 'reason' is invalid digit in binary number, and the 'message' is more
// information about the error. Here the message is just the incorrect digit.
static const char *ErrorCode_strs[] = {
	[STARLARK_ERRORCODE_INVALID] = "INVALID ERRORCODE:",
	[STARLARK_ERRORCODE_BINARY_NUMBER_NO_DIGITS] =
		"binary number has no digits:",
	[STARLARK_ERRORCODE_OCTAL_NUMBER_NO_DIGITS] =
		"octal number has no digits:",
	[STARLARK_ERRORCODE_HEX_NUMBER_NO_DIGITS] =
		"hexadecimal number has no digits:",
	[STARLARK_ERRORCODE_NUMBER_CONSECUTIVE_UNDERSCORES] =
		("cannot have more than one consecutive underscore ('_') in "
		 "number literal"),
	[STARLARK_ERRORCODE_BINARY_NUMBER_INVALID_DIGIT] =
		"invalid digit in binary number:",
	[STARLARK_ERRORCODE_OCTAL_NUMBER_INVALID_DIGIT] =
		"invalid digit in octal number:",
	[STARLARK_ERRORCODE_HEX_NUMBER_INVALID_DIGIT] =
		"invalid digit in hexadecimal number:",
	[STARLARK_ERRORCODE_UNEXPECTED_SYMBOL] = "unexpected symbol:",
	[STARLARK_ERRORCODE_INVALID_UTF8] = "invalid utf-8 character:",
	[STARLARK_ERRORCODE_NEWLINE_IN_STRING] = "unexpected newline in string",
	[STARLARK_ERRORCODE_NEWLINE_IN_RAWBYTES] = ("unexpected newline in "
						    "raw byte string"),
	[STARLARK_ERRORCODE_NEWLINE_IN_BYTES] = ("unexpected newline in byte "
						 "string"),
	[STARLARK_ERRORCODE_NEWLINE_IN_RAW_STRING] = ("unexpected newline in "
						      "raw string"),
	[STARLARK_ERRORCODE_STRING_EOF] = "unexpected end of file in string",
	[STARLARK_ERRORCODE_RAWBYTES_EOF] = ("unexpected end of file in raw "
					     "byte string"),
	[STARLARK_ERRORCODE_BYTES_EOF] = ("unexpected end of file in byte "
					  "string"),
	[STARLARK_ERRORCODE_RAW_STRING_EOF] =
		"unexpected end of file in raw string",
	[STARLARK_ERRORCODE_INVALID_BYTES_CHAR] = ("invalid character in "
						   "byte string:"),
	[STARLARK_ERRORCODE_EXPECTED_IDENT] = "expected identifier, found",
	[STARLARK_ERRORCODE_FLOAT_INVALID] = "invalid float:",
	[STARLARK_ERRORCODE_INT_INVALID] = "invalid int:",
	[STARLARK_ERRORCODE_FLOAT_TOO_BIG] = "float too big for 64 bits:",
	[STARLARK_ERRORCODE_INVALID_ESCAPE] = "invalid escape:",
};

void starlark_Context_finish(struct starlark_Context *ctx)
{
	if (ctx == NULL) {
		return;
	}

	free(ctx->errs.codes);
	strpool_finish(&ctx->strpool);
}

void starlark_error_dump(FILE *f, const char *filename, const size_t buf_len,
			 const uint8_t *buf, const struct starlark_Error err)
{
	size_t line_pos = 0;
	size_t line = lineno(buf_len, buf, err.start, &line_pos);
	fprintf(f, "%s:%zu:%zu: %s", filename, line, line_pos,
		ErrorCode_strs[err.code]);
	if (strcmp("", err.msg) != 0) {
		fprintf(f, " %s\n", err.msg);
	} else {
		fprintf(f, "\n");
	}
}

void starlark_errors_dump(struct starlark_Context *ctx, FILE *f)
{
	struct starlark_Error err = { 0 };
	for (size_t i = 0; i < ctx->errs_len; i += 1) {
		err.code = ctx->errs.codes[i];
		err.msg = strpool_get(&ctx->strpool, ctx->errs.msgs[i]);
		if (err.msg == NULL) {
			panic("starlark_errors_dump got invalid "
			      "handle: %" PRId64,
			      ctx->errs.msgs[i]);
		}

		err.start = ctx->errs.starts[i];
		// printf("err msg handle = %lu\n", ctx->errs.msgs[i]);
		const char *name = strpool_get(&ctx->strpool, ctx->name);
		if (name == NULL) {
			panic("starlark_errors_dump has invalid "
			      "name: %" PRId64,
			      ctx->name);
		}

		starlark_error_dump(f, name, ctx->src_len, ctx->src, err);
	}
}
