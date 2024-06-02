#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "util/lineno.h"

// puts the line position in line_pos if line_pos != NULL.
size_t lineno(const size_t buf_len, const uint8_t *buf, const size_t pos,
	      size_t *line_pos)
{
	assert(buf != NULL);

	size_t line = 1;
	size_t line_start = 0;
	for (size_t i = 0; i < buf_len; i += 1) {
		if (i == pos) {
			break;
		}
		// TODO: is this just \n or \r\n on windows?
		if (buf[i] == u8"\n"[0]) {
			line_start = i;
			line += 1;
			continue;
		}
	}

	if (line_pos != NULL) {
		*line_pos = pos + 1 - line_start;
	}
	return line;
}

char *get_line_frompos(const size_t buf_len, const uint8_t *buf,
		       const size_t pos)
{
	return get_line(buf_len, buf,
			lineno(buf_len, buf, lineno(buf_len, buf, pos, NULL),
			       NULL));
}

char *get_line(const size_t buf_len, const uint8_t *buf, const size_t line)
{
	assert(buf != NULL);
	size_t current = 1;
	const size_t linepos = line;

	for (size_t i = 0; i < buf_len; i += 1) {
		if (buf[i] == u8"\n"[0]) {
			current += 1;
			continue;
		}

		if (current != linepos) {
			continue;
		}

		const size_t line_start = i;
		size_t line_len = 0;
		for (size_t j = i + 1; j < buf_len; j += 1) {
			if (buf[j] == u8"\n"[0]) {
				break;
			}
			line_len += 1;
		}

		char *result = calloc(line_len + 2, 1);
		memcpy(result, &buf[line_start], line_len + 1);
		return result;
	}

	char *result = calloc(1, 1);
	return result;
}
