#include "starlark/parse.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "starlark/common.h"
#include "starlark/lex.h"
#include "starlark/parse.h"
#include "util/panic.h"
#include "util/io.h"

// Currently this just dumps the tokens.

int main(int argc, char **argv)
{
	FILE *f = stdin;
	if (argc > 1) {
		f = fopen(argv[1], "rb");
		if (f == NULL) {
			panic("error opening '%s': %s", argv[1],
			      strerror(errno));
		}
	}

	uint8_t *buf = readfull(f);
	size_t len = strlen((char *)buf);
	if (buf == NULL || len > SIZE_MAX) {
		exit(EXIT_FAILURE);
	}

	struct starlark_Context ctx = { 0 };
	struct starlark_Lexer l = { 0 };
	int ret = starlark_lex(&ctx, "<stdin>", len, buf, &l);
	if (ret != 0) {
		panic("starlark_lex returned: %d", ret);
	}
	struct starlark_Parser p = { 0 };
	ret = starlark_parse_tokens(&ctx, &l, &p);
	if (ret != 0) {
		panic("starlark_parse_tokens returned: %d", ret);
	}

	puts("===TOKENS===");
	starlark_tokens_dump(&l, stdout);
	puts("====AST=====");
	starlark_ast_dump(&p, stdout);
	puts("===ERRORS===");
	starlark_errors_dump(&ctx, stdout);

	free(buf);
	starlark_Parser_finish(&p);
	starlark_Lexer_finish(&l);
	starlark_Context_finish(&ctx);
}
