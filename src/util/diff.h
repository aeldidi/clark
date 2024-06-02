#ifndef UTIL_DIFF_H
#define UTIL_DIFF_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Returns true if the two buffers are equal, or false if not.
// If they differ, places the line which they differ into out.
bool diff(const size_t a_len, const uint8_t *a, const size_t b_len,
	  const uint8_t *b, size_t *out);

// Prints out the lines which the buffers differ. i is the value placed in out
// from diff().
void diff_fwrite(FILE *f, const size_t a_len, const uint8_t *a,
		 const size_t b_len, const uint8_t *b, const size_t i);

#endif // UTIL_DIFF_H
