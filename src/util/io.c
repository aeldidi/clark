#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "util/io.h"

uint8_t *readfull(FILE *f)
{
	uint8_t *result = calloc(2, 1);
	if (result == NULL) {
		return result;
	}

	int c = 0;
	size_t i = 0;
	for (;;) {
		c = fgetc(f);
		if (c == EOF) {
			break;
		}
		result[i] = c & 0xff;
		result[i + 1] = '\0';
		i += 1;
		void *ptr = realloc(result, i + 2);
		if (ptr == NULL) {
			free(result);
			return NULL;
		}
		result = ptr;
	}

	return result;
}
