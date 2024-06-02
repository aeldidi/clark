#include <stdarg.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

static char *vformat(const char *fmt, va_list args)
{
	// The v*printf functions destroy the va_list after use.
	// So I have to copy the va_list for the second call or we segfault.
	va_list ap;
	va_copy(ap, args);
	int len = vsnprintf(NULL, 0, fmt, args);
	if (len < 0 || len == INT_MAX) {
		return NULL;
	}

	char *result = calloc(len + 1, 1);
	if (result == NULL) {
		return NULL;
	}

	(void)vsnprintf(result, len + 1, fmt, ap);
	va_end(ap);
	return result;
}

char *format(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char *result = vformat(fmt, ap);
	va_end(ap);
	return result;
}
