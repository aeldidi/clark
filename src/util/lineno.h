#ifndef UTIL_LINENO_H
#define UTIL_LINENO_H
#include <stddef.h>
#include <stdint.h>

// puts the line position in line_pos if line_pos != NULL.
size_t lineno(const size_t buf_len, const uint8_t *buf, const size_t pos,
	      size_t *line_pos);

// Returns a dynamically allocated string of the line.
char *get_line(const size_t buf_len, const uint8_t *buf, const size_t line);

// Returns a dynamically allocated string of the line.
char *get_line_frompos(const size_t buf_len, const uint8_t *buf,
		       const size_t pos);

#endif // UTIL_LINENO_H
