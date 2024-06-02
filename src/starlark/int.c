#include <stdbool.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tommath.h"

#include "util/panic.h"
#include "starlark/int.h"
#include "starlark/common.h"

static_assert(MP_DIGIT_BIT >= 60, "MP_DIGIT_BIT is too small. Clark only "
				  "supports libtommath configurations with at "
				  "least 61-bit exponents");

struct starlark_Int {
	mp_int value;
};

struct starlark_Int *Int_create()
{
	struct starlark_Int *result = malloc(sizeof(mp_int));
	if (result == NULL) {
		return NULL;
	}

	int ret = mp_init(&result->value);
	if (ret != MP_OKAY) {
		free(result);
		return NULL;
	}

	return result;
}

void Int_set_u32(struct starlark_Int *a, const uint32_t b)
{
	assert(a != NULL);

	mp_set_u32(&a->value, b);
}

struct starlark_Int *Int_create_digits(const size_t digits)
{
	struct starlark_Int *result = malloc(sizeof(mp_int));
	if (result == NULL) {
		return NULL;
	}

	int ret = mp_init_size(&result->value, digits);
	if (ret != MP_OKAY) {
		free(result);
		return NULL;
	}

	return result;
}

int Int_add_u32(const struct starlark_Int *a, const uint32_t b,
		struct starlark_Int *c)
{
	assert(a != NULL);
	assert(c != NULL);
	mp_int tmp;
	if (mp_init(&tmp) != MP_OKAY) {
		return STARLARK_ERROR_OOM;
	}

	mp_set_u32(&tmp, b);

	if (mp_add(&a->value, &tmp, &c->value) != MP_OKAY) {
		mp_clear(&tmp);
		return STARLARK_ERROR_OOM;
	}

	mp_clear(&tmp);
	return 0;
}

void Int_destroy(const struct starlark_Int *i)
{
	if (i == NULL) {
		return;
	}

	mp_clear((mp_int *)&i->value);
	free((struct starlark_Int *)i);
}

char *Int_to_str(const struct starlark_Int *i, const int base)
{
	assert(i != NULL);
	assert(base > 2);
	assert(base < 64);
	int str_len = 0;
	mp_err ret = mp_radix_size(&i->value, base, &str_len);
	if (ret != MP_OKAY) {
		return NULL;
	}

	size_t len = str_len;
	if (base == 16 || base == 8 || base == 2) {
		len += 2;
	}

	char *result = malloc(len);
	if (result == NULL) {
		return NULL;
	}

	char *ptr = result;

	if (base == 16) {
		memcpy(result, u8"0x", 2);
		ptr += 2;
	} else if (base == 8) {
		memcpy(result, u8"0o", 2);
		ptr += 2;
	} else if (base == 2) {
		memcpy(result, u8"0b", 2);
		ptr += 2;
	}

	size_t written = 0;
	ret = mp_to_radix(&i->value, ptr, str_len, &written, base);
	if (ret != MP_OKAY || written != (size_t)str_len) {
		free(result);
		return NULL;
	}

	return result;
}

struct starlark_Int *Int_from_str(const char *str, const int base)
{
	assert(str != NULL);
	struct starlark_Int *result = Int_create();
	if (result == NULL) {
		return NULL;
	}

	mp_err ret = mp_read_radix(&result->value, str, base);
	if (ret != MP_OKAY) {
		return NULL;
	}

	return result;
}

int Int_add(const struct starlark_Int *a, const struct starlark_Int *b,
	    struct starlark_Int *c)
{
	assert(a != NULL);
	assert(b != NULL);
	assert(c != NULL);
	if (mp_add(&a->value, &b->value, &c->value) != MP_OKAY) {
		return STARLARK_ERROR_OOM;
	}

	return 0;
}

int Int_mul(const struct starlark_Int *a, const struct starlark_Int *b,
	    struct starlark_Int *c)
{
	assert(a != NULL);
	assert(b != NULL);
	assert(c != NULL);
	if (mp_mul(&a->value, &b->value, &c->value) != MP_OKAY) {
		return STARLARK_ERROR_OOM;
	}

	return 0;
}

int Int_pow_u32(const struct starlark_Int *a, const uint32_t b,
		struct starlark_Int *c)
{
	assert(a != NULL);
	assert(c != NULL);
	if (mp_expt_u32(&a->value, b, &c->value) != MP_OKAY) {
		return STARLARK_ERROR_OOM;
	}

	return 0;
}
