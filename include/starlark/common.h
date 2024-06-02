#ifndef STARLARK_COMMON_H
#define STARLARK_COMMON_H
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

// Each handle is an index into buffer. Index 0 points to an empty string.
struct starlark_Strpool {
	// Insert-only hashtable
	struct {
		size_t len;
		size_t cap;
		int64_t *hashes;
		int64_t *handles;
		size_t *lens;
	} table;

	struct {
		size_t len;
		size_t cap;
		char *ptr;
	} buffer;
};

#ifndef __STDC_IEC_559__
#error "clark requires double to be an IEEE-754 binary64 floating-point"
#endif

enum {
	STARLARK_ERROR_UNINITIALIZED_CONTEXT = -1,
	STARLARK_ERROR_OOM = -2,
	STARLARK_ERROR_TOOBIG = -3,
	STARLARK_ERROR_NOTSUPPORTED = -4,
};

enum starlark_ErrorCode {
	STARLARK_ERRORCODE_INVALID = 0,
	STARLARK_ERRORCODE_BINARY_NUMBER_NO_DIGITS,
	STARLARK_ERRORCODE_OCTAL_NUMBER_NO_DIGITS,
	STARLARK_ERRORCODE_HEX_NUMBER_NO_DIGITS,
	STARLARK_ERRORCODE_NUMBER_CONSECUTIVE_UNDERSCORES,
	STARLARK_ERRORCODE_BINARY_NUMBER_INVALID_DIGIT,
	STARLARK_ERRORCODE_OCTAL_NUMBER_INVALID_DIGIT,
	STARLARK_ERRORCODE_HEX_NUMBER_INVALID_DIGIT,
	STARLARK_ERRORCODE_UNEXPECTED_SYMBOL,
	STARLARK_ERRORCODE_INVALID_UTF8,
	STARLARK_ERRORCODE_NEWLINE_IN_STRING,
	STARLARK_ERRORCODE_NEWLINE_IN_RAWBYTES,
	STARLARK_ERRORCODE_NEWLINE_IN_BYTES,
	STARLARK_ERRORCODE_NEWLINE_IN_RAW_STRING,
	STARLARK_ERRORCODE_STRING_EOF,
	STARLARK_ERRORCODE_RAWBYTES_EOF,
	STARLARK_ERRORCODE_BYTES_EOF,
	STARLARK_ERRORCODE_RAW_STRING_EOF,
	STARLARK_ERRORCODE_INVALID_BYTES_CHAR,
	STARLARK_ERRORCODE_EXPECTED_IDENT,
	STARLARK_ERRORCODE_FLOAT_INVALID,
	STARLARK_ERRORCODE_INT_INVALID,
	STARLARK_ERRORCODE_FLOAT_TOO_BIG,
	STARLARK_ERRORCODE_INVALID_ESCAPE,
};

struct starlark_Int;

// A starlark_Error consists of an error code, line and column, and a message.
// For example, an error message like this:
//
// msg is heap allocated.
// Use starlark_errors_free to free all error messages.
struct starlark_Error {
	enum starlark_ErrorCode code;
	size_t start;
	const char *msg;
};

// TODO: implement this
struct starlark_Dict;

struct starlark_Context {
	struct starlark_Strpool strpool;

	int64_t name;
	size_t src_len;
	const uint8_t *src;

	int8_t err;
	size_t errs_len;
	size_t errs_cap;

	size_t srcs_len;
	struct starlark_Dict *globals;
	struct {
		enum starlark_ErrorCode *codes;
		size_t *starts;
		// handle into strpool
		int64_t *msgs;
	} errs;
};

// Attempts to set the configuration key to the given value.
// Returns 0 if set successfully, and a nonzero error code otherwise.
STARLARK_PUBLIC
int starlark_config_set(struct starlark_Context *ctx, const char *key,
			const char *value);

// Initializes a starlark_Context with the given name from the starlark source
// contained in the first src_len bytes of src.
STARLARK_PUBLIC
int starlark_exec(struct starlark_Context *ctx, const char *name,
		  const size_t src_len, const uint8_t *src);

// Initializes a starlark_Context from the file with the given filename.
STARLARK_PUBLIC
int starlark_execfile(struct starlark_Context *ctx, const char *filename);

STARLARK_PUBLIC
void starlark_error_dump(FILE *f, const char *filename, const size_t buf_len,
			 const uint8_t *buf, const struct starlark_Error err);

STARLARK_PUBLIC
void starlark_errors_dump(struct starlark_Context *ctx, FILE *f);

STARLARK_PUBLIC
void starlark_Context_finish(struct starlark_Context *ctx);

#endif // STARLARK_COMMON_H
