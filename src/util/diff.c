#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util/diff.h"
#include "util/common.h"
#include "util/lineno.h"

bool diff(const size_t a_len, const uint8_t *a, const size_t b_len,
	  const uint8_t *b, size_t *out)
{
	assert(a != NULL);
	assert(b != NULL);
	assert(out != NULL);

	size_t a_lines = lineno(a_len, a, a_len - 1, NULL);
	size_t b_lines = lineno(b_len, b, b_len - 1, NULL);

	if (a_lines != b_lines) {
		*out = MIN(a_lines, b_lines) + 1;
		return false;
	}

	for (size_t i = 0; i < MAX(a_lines, b_lines); i += 1) {
		char *a_line = get_line(a_len, a, i);
		char *b_line = get_line(b_len, b, i);
		size_t a_linelen = strlen(a_line);
		size_t b_linelen = strlen(b_line);
		for (size_t j = 0; j < MAX(a_linelen, b_linelen); j += 1) {
			if (a[j] != b[j]) {
				*out = j;
				free(a_line);
				free(b_line);
				return false;
			}
		}

		free(a_line);
		free(b_line);
	}

	return true;
}

void diff_fwrite(FILE *f, const size_t a_len, const uint8_t *a,
		 const size_t b_len, const uint8_t *b, const size_t i)
{
	assert(a != NULL);
	assert(b != NULL);
	assert(f != NULL);

	size_t a_lines = lineno(a_len, a, a_len - 1, NULL);
	size_t b_lines = lineno(b_len, b, b_len - 1, NULL);

	if (a_lines != b_lines) {
		fprintf(f,
			"output has %zu lines when %zu lines were expected\n",
			a_lines, b_lines);
		fprintf(f, "output:\n%.*s", (int)a_len, a);
		return;
	}

	char *l1 = get_line(a_len, a, a_lines);
	char *l2 = get_line(b_len, b, a_lines);

	fprintf(f, "output differs at line %zu\n", i);
	fprintf(f, "output:   %s\n", l1);
	fprintf(f, "expected: %s\n", l2);
	free(l1);
	free(l2);
}
