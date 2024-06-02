#ifndef UTIL_IO_H
#define UTIL_IO_H
#include <stdint.h>
#include <stdio.h>

// Reads all of f into a heap allocated buffer.
// Returns NULL on failure.
uint8_t *readfull(FILE *f);

#endif // UTIL_IO_H
