// SPDX-License-Identifier: 0BSD
// Copyright (C) 2022 Ayman El Didi
#ifndef UTF8_H
#define UTF8_H

// encoding/utf8.h provides functions for encoding, decoding, and validating
// UTF-8 encoded text as defined in RFC 3629, which can be found at:
// https://datatracker.ietf.org/doc/html/rfc3629
//
// This code is adapted from https://git.sr.ht/~aeldidi/libencoding.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UTF8_ERROR (UINT32_MAX)
#define UTF8_EOF (UINT32_MAX - 1)

// Some UTF-8 characters for use in case statements
#define UTF8_LPAREN (0x28)
#define UTF8_RPAREN (0x29)
#define UTF8_LBRACKET (0x5b)
#define UTF8_RBRACKET (0x5d)
#define UTF8_LBRACE (0x7b)
#define UTF8_RBRACE (0x7d)
#define UTF8_PLUS (0x2b)
#define UTF8_MINUS (0x2d)
#define UTF8_MUL (0x2a)
#define UTF8_MOD (0x25)
#define UTF8_DIV (0x2f)
#define UTF8_BITAND (0x26)
#define UTF8_BITOR (0x7c)
#define UTF8_XOR (0x5e)
#define UTF8_BITNOT (0x7e)
#define UTF8_EQ (0x3d)
#define UTF8_LESS (0x3c)
#define UTF8_GREATER (0x3e)
#define UTF8_EXCLAMATION (0x21)
#define UTF8_COLON (0x3a)
#define UTF8_COMMA (0x2c)
#define UTF8_SEMICOLON (0x3b)
#define UTF8_DOT (0x2e)
#define UTF8_NEWLINE (0x0a)
#define UTF8_SINGLEQUOTE (0x27)
#define UTF8_DOUBLEQUOTE (0x22)
#define UTF8_BACKSLASH (0x5c)
#define UTF8_POUND (0x23)
#define UTF8_UNDERSCORE (0x5f)

// utf8_codepoint_encode encodes the codepoint cp into the string out writing
// at most out_len bytes. If cp is an invalid codepoint, the function tries to
// encode the Unicode Replacement Character (U+FFFD).
//
// On success, returns the number of bytes written.
// On failure, panics.
void utf8_codepoint_encode(const uint32_t codepoint, const size_t out_len,
			   uint8_t *out);

// utf8_codepoint_decode decodes the first codepoint found in str when reading
// at most len bytes. If size is not NULL, *size is set to the width of the
// read character.
//
// On success, returns the unicode codepoint number, i.e the number of the
// form `U+XXXXXX` for the decoded codepoint.
// On failure returns UTF8_ERROR.
// If current >= str_len, returns UTF8_EOF.
uint32_t utf8_codepoint_decode(const size_t str_len, const uint8_t *str,
			       const size_t current, size_t *size);

// utf8_codepoint_size returns the size of the codepoint cp in bytes when
// encoded as UTF-8.
size_t utf8_codepoint_size(const uint32_t cp);

bool utf8_codepoint_valid(const uint32_t cp);

#endif // UTF8_H
