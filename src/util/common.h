#ifndef UTIL_COMMON_H
#define UTIL_COMMON_H

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

// returns the next aligned address
#define ALIGN_UP(ptr, alignment) \
	((ptr) + ((alignment) - ((uintptr_t)(ptr) % (alignment))))

#ifdef __cplusplus
#define noreturn [[noreturn]]
#else
#include <stdnoreturn.h>
#endif // __cplusplus

#endif // UTIL_COMMON_H
