#ifndef UTIL_FNV_1A_H
#define UTIL_FNV_1A_H
#include <stdint.h>
#include <stddef.h>

// calculates the fnv-1a hash of the string str.
uint64_t fnv_1a_str(const char *str);

// calculates the fnv-1a hash of the first buf_len bytes of the buffer buf.
uint64_t fnv_1a(const size_t buf_len, const uint8_t *buf);

#endif // UTIL_FNV_1A_H
