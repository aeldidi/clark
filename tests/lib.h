#ifndef TESTS_LIB_H
#define TESTS_LIB_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "util/panic.h"

extern int errno;

static FILE *open_with_suffix(const char *filename, const char *suffix,
			      const char *mode)
{
	errno = 0;
	char *tmp = calloc(strlen(filename) + strlen(suffix) + 1, 1);
	if (filename == NULL) {
		panic("open_with_suffix: %s", strerror(errno));
	}

	memcpy(tmp, filename, strlen(filename));
	memcpy(&tmp[strlen(filename)], suffix, strlen(suffix));

	errno = 0;
	FILE *result = fopen(tmp, mode);
	if (result == NULL) {
		panic("couldn't open file '%s': %s", tmp, strerror(errno));
	}

	free(tmp);

	return result;
}

#endif // TESTS_LIB_H
