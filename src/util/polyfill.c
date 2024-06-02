#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "util/polyfill.h"

char *strdup(const char *s)
{
	size_t len = strlen(s);
	char *result = calloc(len + 1, 1);
	if (result == NULL) {
		return NULL;
	}
	memcpy(result, s, len);
	return result;
}
