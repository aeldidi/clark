#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "util/common.h"
#include "util/panic.h"

noreturn void panic_impl(const char *filename, const int line, const char *fmt,
			 ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "panic:%s:%d ", filename, line);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);

	exit(EXIT_FAILURE);
}
