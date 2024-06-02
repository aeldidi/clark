#ifndef UTIL_PANIC_H
#define UTIL_PANIC_H

#include "util/common.h"

#define panic(...) panic_impl(__FILE__, __LINE__, __VA_ARGS__)
#define unimplemented() \
	panic_impl(__FILE__, __LINE__, "reached unimplemented code")
#define unreachable() panic_impl(__FILE__, __LINE__, "reached unreachable code")

noreturn void panic_impl(const char *filename, const int line, const char *fmt,
			 ...);

#endif // UTIL_PANIC_H
