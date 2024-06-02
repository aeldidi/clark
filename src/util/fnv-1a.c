#include <stdint.h>
#include <stddef.h>

#define FNV_64_PRIME (UINT64_C(0x100000001b3))
#define FNV_64_INITIAL (UINT64_C(0xcbf29ce484222325))

uint64_t fnv_1a_str(const char *str)
{
	const char *ptr = str;

	uint64_t hash = FNV_64_INITIAL;

	while (*ptr != '\0') {
		hash = (hash ^ *ptr) * FNV_64_PRIME;
		ptr += 1;
	}

	return hash;
}

uint64_t fnv_1a(const size_t buf_len, const uint8_t *buf)
{
	uint64_t hash = FNV_64_INITIAL;

	for (size_t i = 0; i < buf_len; i += 1) {
		hash = (hash ^ buf[i]) * FNV_64_PRIME;
	}

	return hash;
}
