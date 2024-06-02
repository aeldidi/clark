#include <stdint.h>
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "starlark/common.h"
#include "starlark/lex.h"
#include "starlark/parse.h"
#include "util/common.h"
#include "util/panic.h"
#include "util/io.h"
#include "util/lineno.h"
#include "util/diff.h"
#include "../lib.h"

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: runner file.txt\n");
		return EXIT_FAILURE;
	}

	errno = 0;
	FILE *f = fopen(argv[1], "rb");
	if (f == NULL) {
		panic("error reading file '%s': %s", argv[1], strerror(errno));
	}
	uint8_t *input_buf = readfull(f);

	if (ferror(f)) {
		panic("error reading file '%s': %s", argv[1], strerror(errno));
	}
	fclose(f);
	size_t input_len = strlen((const char *)input_buf);
	assert(input_len < SIZE_MAX);

	// Initializing Starlark

	struct starlark_Lexer l = { 0 };
	struct starlark_Context ctx = { 0 };
	int ret = starlark_lex(&ctx, "<stdin>", input_len, input_buf, &l);
	if (ret != 0) {
		panic("starlark_lex returned: %d", ret);
	}

	struct starlark_Parser p = { 0 };
	ret = starlark_parse_tokens(&ctx, &l, &p);

	// Dump tokens and errors into tmpfile, then read into a buffer.

	errno = 0;
	f = tmpfile();
	if (f == NULL) {
		panic("couldn't make a tempfile: %s", strerror(errno));
	}

	starlark_ast_dump(&p, f);
	starlark_errors_dump(&ctx, f);

	fseek(f, 0, SEEK_SET);
	uint8_t *tok_buf = readfull(f);

	if (ferror(f)) {
		panic("error reading temp file: %s", strerror(errno));
	}
	fclose(f);
	size_t tok_len = strlen((const char *)tok_buf);

	f = open_with_suffix(argv[1], ".expect", "rb");
	uint8_t *expect_buf = readfull(f);

	if (ferror(f)) {
		panic("error reading file '%s': %s", argv[1], strerror(errno));
	}
	fclose(f);
	size_t expect_len = strlen((const char *)expect_buf);

	// Diff

	assert(tok_len < SIZE_MAX);
	assert(expect_len < SIZE_MAX);

	size_t diff_idx = 0;
	int status = EXIT_SUCCESS;
	if (!diff(tok_len, tok_buf, expect_len, expect_buf, &diff_idx)) {
		diff_fwrite(stderr, tok_len, tok_buf, expect_len, expect_buf,
			    diff_idx);
		status = EXIT_FAILURE;
	}

	free(input_buf);
	free(expect_buf);
	free(tok_buf);
	starlark_Parser_finish(&p);
	starlark_Lexer_finish(&l);
	starlark_Context_finish(&ctx);

	return status;
}
