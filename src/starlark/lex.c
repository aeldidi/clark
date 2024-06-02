#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdalign.h>

#include "starlark/common.h"
#include "starlark/lex.h"
#include "starlark/util.h"
#include "starlark/strpool.h"
#include "utf8/utf8.h"
#include "util/fmt.h"
#include "util/polyfill.h"
#include "util/common.h"

static bool lexer_append(struct starlark_Lexer *in,
			 const enum starlark_TokenTag tag, const size_t len)
{
	assert(in != NULL);
	if (in->toks_len + 1 >= in->toks_cap) {
		// TODO: check for overflow
		const size_t cap = (in->toks_cap + 16) * 1.5;
		size_t tags_len = cap * sizeof(in->toks.tags[0]);
		size_t starts_len = cap * sizeof(in->toks.starts[0]);
		size_t ends_len = cap * sizeof(in->toks.ends[0]);
		uint8_t *ptr =
			realloc(in->toks.tags,
				tags_len + starts_len + ends_len +
					alignof(enum starlark_TokenTag) +
					alignof(size_t) + alignof(size_t));
		if (ptr == NULL) {
			return false;
		}

		in->toks.tags = (enum starlark_TokenTag *)ptr;
		in->toks.starts =
			(size_t *)ALIGN_UP(ptr + tags_len, alignof(size_t));
		in->toks.ends = (size_t *)ALIGN_UP(ptr + tags_len + starts_len,
						   alignof(size_t));
		in->toks_cap = cap;
	}

	in->toks.tags[in->toks_len] = tag;
	in->toks.starts[in->toks_len] = in->idx + 1;
	in->toks.ends[in->toks_len] = in->idx + 1 + len;
	in->toks_len += 1;
	return true;
}

static bool in_array(const uint8_t needle, const uint8_t haystack_len,
		     const uint8_t *haystack)
{
	for (uint8_t i = 0; i < haystack_len; i += 1) {
		if (haystack[i] != needle) {
			continue;
		}

		return true;
	}

	return false;
}

static uint32_t peek_impl(struct starlark_Lexer *in, size_t *len)
{
	uint32_t result = utf8_codepoint_decode(in->ctx->src_len, in->ctx->src,
						in->idx + 1, len);
	return result;
}

static uint32_t peek(struct starlark_Lexer *l)
{
	assert(l != NULL);
	size_t len = 0;
	return peek_impl(l, &len);
}

static uint32_t next(struct starlark_Lexer *l)
{
	assert(l != NULL);
	size_t len = 0;
	uint32_t result = peek_impl(l, &len);
	if (result != UTF8_EOF) {
		// TODO: overflow check
		l->idx += len;
	}

	return result;
}

static void advance(struct starlark_Lexer *l)
{
	assert(l != NULL);
	size_t len = 0;
	(void)peek_impl(l, &len);
	l->idx += len;
}

static bool expect(struct starlark_Lexer *l, const char *next)
{
	assert(l != NULL);
	assert(next != NULL);
	const size_t len = strlen(next);
	if (l->ctx->src_len - (l->idx + 1) < len) {
		return false;
	}
	return memcmp(next, &l->ctx->src[l->idx + 1], len) == 0;
}

static bool consume(struct starlark_Lexer *l, const char *next)
{
	if (!expect(l, next)) {
		return false;
	}

	l->idx += strlen(next);
	return true;
}

static char *quoted_char_string(struct starlark_Lexer *in)
{
	size_t len2 = 0;
	uint32_t tmp = peek_impl(in, &len2);
	if (tmp == UTF8_ERROR) {
		char buf[30] = { 0 };
		buf[0] = '\'';

		for (size_t i = 0; i < len2; i += 1) {
			snprintf(&buf[1 + 4 * i], 5 - i, "\\x%x",
				 in->ctx->src[in->idx + 1]);
		}

		buf[len2 * 4 + 1] = '\'';
		return strdup(buf);
	}

	return format("'%.*s'", len2, &in->ctx->src[in->idx + 1]);
}

// Consumes any of the characters in str, returning true if one was next.
static bool consume_one(struct starlark_Lexer *l, const char *str)
{
	size_t len = 0;
	uint32_t c = peek_impl(l, &len);

	if (c == UTF8_ERROR) {
		struct starlark_Error err = {
			.code = STARLARK_ERRORCODE_INVALID_UTF8,
			.msg = quoted_char_string(l),
			.start = l->idx + 1,
		};
		if (!err_append(l->ctx, err) ||
		    !lexer_append(l, STARLARK_TOKEN_ERROR, l->idx + len)) {
			l->ctx->err = STARLARK_ERROR_OOM;
		}
		advance(l);
		return false;
	}

	if (!in_array(c & 0xff, strlen(str), (uint8_t *)str)) {
		return false;
	}

	advance(l);
	return true;
}

static bool consume_run(struct starlark_Lexer *l, const char *str)
{
	if (!consume_one(l, str)) {
		return false;
	}

	while (consume_one(l, str)) {
	}

	return true;
}

// A StateFn is a function that returns a StateFn. This struct is just a
// wrapper since C doesn't allow recursive typedefs.
struct StateFn {
	struct StateFn (*func)(struct starlark_Lexer *l);
};

// The default lex state. All other states branch off from this one.
static struct StateFn text(struct starlark_Lexer *l);

static struct StateFn push(struct starlark_Lexer *l,
			   const enum starlark_TokenTag tag, const size_t len)
{
	if (l->ctx->err) {
		return (struct StateFn){ NULL };
	}

	if (!lexer_append(l, tag, len)) {
		l->ctx->err = STARLARK_ERROR_OOM;
		return (struct StateFn){ NULL };
	}

	return (struct StateFn){ text };
}

static struct StateFn push_err(struct starlark_Lexer *l,
			       const struct starlark_Error err,
			       const size_t len)
{
	if (l->ctx->err) {
		return (struct StateFn){ NULL };
	}

	if (!err_append(l->ctx, err)) {
		l->ctx->err = STARLARK_ERROR_OOM;
		return (struct StateFn){ NULL };
	}

	return push(l, STARLARK_TOKEN_ERROR, len);
}

static void skip_whitespace(struct starlark_Lexer *l)
{
	uint32_t c = 0;
	for (;;) {
		c = peek(l);

		// Comment
		if (c == UTF8_POUND) {
			(void)push(l, STARLARK_TOKEN_COMMENT, 0);
			advance(l);
			for (;;) {
				c = peek(l);

				if (c == UTF8_ERROR) {
					struct starlark_Error err = {
						.code = STARLARK_ERRORCODE_INVALID_UTF8,
						.msg = quoted_char_string(l),
						.start = l->idx + 1,
					};
					if (!err_append(l->ctx, err)) {
						l->ctx->err =
							STARLARK_ERROR_OOM;
					}
				}

				if (c == UTF8_EOF) {
					return;
				}

				if (c == UTF8_NEWLINE) {
					advance(l);
					break;
				}

				advance(l);
			}
		}

		if (c == UTF8_EOF || !starlark_isspace(c)) {
			return;
		}
		advance(l);
	}
}

// A number literal can be of arbitrary length, since we support constants of
// any length which are automatically converted to the type they're assigned
// to.
//
// We detect invalid number literals in the parser.
static struct StateFn number(struct starlark_Lexer *l)
{
	size_t initial = l->idx;
	bool decimal = true;
	const char *hex_chars = u8"0123456789abcdefABDCEF";
	const char *octal_chars = u8"01234567";
	const char *binary_chars = u8"01";
	const char *chars = u8"0123456789";
	struct StateFn result = push(l, STARLARK_TOKEN_NUMBER, 0);

	if (expect(l, u8"0x") || expect(l, u8"0X")) {
		decimal = false;
		l->idx += 2;
		chars = hex_chars;
	} else if (expect(l, u8"0o") || expect(l, u8"0O")) {
		decimal = false;
		l->idx += 2;
		chars = octal_chars;
	} else if (expect(l, u8"0b") || expect(l, u8"0B")) {
		decimal = false;
		l->idx += 2;
		chars = binary_chars;
	}

	(void)consume_run(l, chars);

	if (decimal) {
		// base 10 numbers might have a decimal or might be in
		// scientific notation

		if (consume_one(l, u8".")) {
			(void)consume_run(l, chars);
		}

		if (consume_one(l, u8"eE")) {
			(void)consume_one(l, u8"+-");
			(void)consume_run(l, chars);
		}
	}

	size_t len = l->idx - initial;
	for (size_t i = l->toks_len - 1; i != SIZE_MAX; i -= 1) {
		if (l->toks.tags[i] != STARLARK_TOKEN_NUMBER) {
			continue;
		}

		l->toks.ends[i] = initial + len + 1;
		break;
	}

	return result;
}

static struct StateFn identifier_or_keyword(struct starlark_Lexer *l)
{
	size_t initial = l->idx;
	uint32_t c = 0;
	for (;;) {
		c = peek(l);
		if (!starlark_isident(c)) {
			break;
		}
		advance(l);
	}

	size_t len = l->idx - initial;
	l->idx = initial;
	struct StateFn result = push(l, STARLARK_TOKEN_IDENT_OR_KEYWORD, len);
	l->idx += len;

	if (c == UTF8_ERROR) {
		result = push_err(
			l,
			(struct starlark_Error){
				.code = STARLARK_ERRORCODE_INVALID_UTF8,
				.msg = quoted_char_string(l),
				.start = l->idx + 1,
			},
			0);
		advance(l);
		return result;
	}

	return result;
}

static struct StateFn string(struct starlark_Lexer *l)
{
	// Escape sequences are validated and handled in parser.
	size_t initial = l->idx;
	bool raw = false;
	bool bytes = false;

	if (consume(l, u8"br") || consume(l, u8"rb")) {
		bytes = true;
		raw = true;
	} else if (consume(l, u8"r")) {
		raw = true;
	} else if (consume(l, u8"b")) {
		bytes = true;
	}

	uint32_t quote_char = next(l);
	bool triple = false;

	if (peek(l) == quote_char) {
		l->idx += 1;
		if (peek(l) == quote_char) {
			l->idx += 1;
			triple = true;
		} else {
			l->idx -= 1;
		}
	}

	uint32_t c = 0;
	for (;;) {
		c = peek(l);

		if (triple) {
			if (quote_char == UTF8_DOUBLEQUOTE &&
			    consume(l, u8"\"\"\"")) {
				break;
			}
			if (quote_char == UTF8_SINGLEQUOTE &&
			    consume(l, u8"'''")) {
				break;
			}

		} else if (c == quote_char) {
			advance(l);
			break;
		}

		if (c == UTF8_EOF || (!triple && c == UTF8_NEWLINE)) {
			break;
		}

		if (bytes && !starlark_isbytes(c)) {
			struct starlark_Error err = {
				.code = STARLARK_ERRORCODE_INVALID_BYTES_CHAR,
				.msg = quoted_char_string(l),
				.start = l->idx + 1,
			};
			if (!err_append(l->ctx, err)) {
				l->ctx->err = STARLARK_ERROR_OOM;
				return (struct StateFn){ NULL };
			}
		}

		if (c == UTF8_BACKSLASH) {
			l->idx += 1;
			if (peek(l) == quote_char) {
				l->idx += 1;
				continue;
			}
			if (!raw && peek(l) == UTF8_NEWLINE) {
				l->idx += 1;
				continue;
			}

			l->idx -= 1;
		}

		advance(l);
	}

	size_t len = l->idx - initial;
	l->idx = initial;
	struct StateFn result = { 0 };
	if (c == UTF8_NEWLINE) {
		// Unclosed string.
		struct starlark_Error err = {
			.code = STARLARK_ERRORCODE_NEWLINE_IN_STRING,
			.msg = NULL,
			.start = l->idx + 1,
		};
		if (bytes && raw) {
			err.code = STARLARK_ERRORCODE_NEWLINE_IN_RAWBYTES;
		} else if (bytes) {
			err.code = STARLARK_ERRORCODE_NEWLINE_IN_BYTES;
		} else if (raw) {
			err.code = STARLARK_ERRORCODE_NEWLINE_IN_RAW_STRING;
		}
		result = push_err(l, err, 0);
	} else if (c == UTF8_EOF) {
		// Unclosed string.
		struct starlark_Error err = {
			.code = STARLARK_ERRORCODE_STRING_EOF,
			.msg = NULL,
			.start = l->idx + 1,
		};
		if (bytes && raw) {
			err.code = STARLARK_ERRORCODE_RAWBYTES_EOF;
		} else if (bytes) {
			err.code = STARLARK_ERRORCODE_BYTES_EOF;
		} else if (raw) {
			err.code = STARLARK_ERRORCODE_RAW_STRING_EOF;
		}
		result = push_err(l, err, 0);
	} else {
		result = push(l, STARLARK_TOKEN_STRING, len);
	}
	l->idx += len;
	return result;
}

static struct StateFn symbol(struct starlark_Lexer *l)
{
	uint32_t c = peek(l);
	struct StateFn result = { 0 };

	// in C11, u8""[0] isn't a constant expression, so to use 'switch' I
	// need to use the binary representation of each character.
	switch (c) {
	case UTF8_LPAREN:
		result = push(l, STARLARK_TOKEN_LPAREN, 1);
		l->idx += 1;
		return result;
	case UTF8_RPAREN:
		result = push(l, STARLARK_TOKEN_RPAREN, 1);
		l->idx += 1;
		return result;
	case UTF8_LBRACKET:
		result = push(l, STARLARK_TOKEN_LBRACKET, 1);
		l->idx += 1;
		return result;
	case UTF8_RBRACKET:
		result = push(l, STARLARK_TOKEN_RBRACKET, 1);
		l->idx += 1;
		return result;
	case UTF8_LBRACE:
		result = push(l, STARLARK_TOKEN_LBRACE, 1);
		l->idx += 1;
		return result;
	case UTF8_RBRACE:
		result = push(l, STARLARK_TOKEN_RBRACE, 1);
		l->idx += 1;
		return result;
	case UTF8_PLUS:
		if (expect(l, u8"+=")) {
			result = push(l, STARLARK_TOKEN_PLUSEQ, 2);
			l->idx += 2;
			return result;
		}

		result = push(l, STARLARK_TOKEN_PLUS, 1);
		l->idx += 1;
		return result;
	case UTF8_MINUS:
		if (expect(l, u8"-=")) {
			result = push(l, STARLARK_TOKEN_MINUSEQ, 2);
			l->idx += 2;
			return result;
		}

		result = push(l, STARLARK_TOKEN_MINUS, 1);
		l->idx += 1;
		return result;
	case UTF8_MUL:
		if (expect(l, u8"**")) {
			result = push(l, STARLARK_TOKEN_EXP, 2);
			l->idx += 2;
			return result;
		}

		if (expect(l, u8"*=")) {
			result = push(l, STARLARK_TOKEN_MULEQ, 2);
			l->idx += 2;
			return result;
		}

		result = push(l, STARLARK_TOKEN_MUL, 1);
		l->idx += 1;
		return result;
	case UTF8_MOD:
		if (expect(l, u8"%=")) {
			result = push(l, STARLARK_TOKEN_MODEQ, 2);
			l->idx += 2;
			return result;
		}

		result = push(l, STARLARK_TOKEN_MOD, 1);
		l->idx += 1;
		return result;
	case UTF8_DIV:
		if (expect(l, u8"//")) {
			result = push(l, STARLARK_TOKEN_DIVINT, 2);
			l->idx += 2;
			return result;
		}

		if (expect(l, u8"/=")) {
			result = push(l, STARLARK_TOKEN_DIVEQ, 2);
			l->idx += 2;
			return result;
		}

		result = push(l, STARLARK_TOKEN_DIV, 1);
		l->idx += 1;
		return result;
	case UTF8_BITAND:
		if (expect(l, u8"&=")) {
			result = push(l, STARLARK_TOKEN_BITANDEQ, 2);
			l->idx += 2;
			return result;
		}

		result = push(l, STARLARK_TOKEN_BITAND, 1);
		l->idx += 1;
		return result;
	case UTF8_BITOR:
		if (expect(l, u8"|=")) {
			result = push(l, STARLARK_TOKEN_BITOREQ, 2);
			l->idx += 2;
			return result;
		}

		result = push(l, STARLARK_TOKEN_BITOR, 1);
		l->idx += 1;
		return result;
	case UTF8_XOR:
		if (expect(l, u8"^=")) {
			result = push(l, STARLARK_TOKEN_XOREQ, 2);
			l->idx += 2;
			return result;
		}

		result = push(l, STARLARK_TOKEN_XOR, 1);
		l->idx += 1;
		return result;
	case UTF8_BITNOT:
		result = push(l, STARLARK_TOKEN_BITNOT, 1);
		l->idx += 1;
		return result;
	case UTF8_EQ:
		if (expect(l, u8"==")) {
			result = push(l, STARLARK_TOKEN_EQ, 2);
			l->idx += 2;
			return result;
		}

		result = push(l, STARLARK_TOKEN_ASSIGN, 1);
		l->idx += 1;
		return result;
	case UTF8_LESS:
		if (expect(l, u8"<<=")) {
			result = push(l, STARLARK_TOKEN_LSHIFTEQ, 3);
			l->idx += 3;
			return result;
		}

		if (expect(l, u8"<<")) {
			result = push(l, STARLARK_TOKEN_LSHIFT, 2);
			l->idx += 2;
			return result;
		}

		if (expect(l, u8"<=")) {
			result = push(l, STARLARK_TOKEN_LEQ, 2);
			l->idx += 2;
			return result;
		}

		result = push(l, STARLARK_TOKEN_LESS, 1);
		l->idx += 1;
		return result;
	case UTF8_GREATER:
		if (expect(l, u8">>=")) {
			result = push(l, STARLARK_TOKEN_RSHIFTEQ, 3);
			l->idx += 3;
			return result;
		}

		if (expect(l, u8">>")) {
			result = push(l, STARLARK_TOKEN_RSHIFT, 2);
			l->idx += 2;
			return result;
		}

		if (expect(l, u8">=")) {
			result = push(l, STARLARK_TOKEN_GEQ, 2);
			l->idx += 2;
			return result;
		}

		result = push(l, STARLARK_TOKEN_GREATER, 1);
		l->idx += 1;
		return result;
	case UTF8_EXCLAMATION:
		if (expect(l, u8"!=")) {
			result = push(l, STARLARK_TOKEN_NOTEQ, 2);
			l->idx += 2;
			return result;
		}

		result = push_err(
			l,
			(struct starlark_Error){
				.code = STARLARK_ERRORCODE_UNEXPECTED_SYMBOL,
				.msg = quoted_char_string(l),
				.start = l->idx + 1,
			},
			1);
		l->idx += 1;
		return result;
	case UTF8_COLON:
		result = push(l, STARLARK_TOKEN_COLON, 1);
		l->idx += 1;
		return result;
	case UTF8_COMMA:
		result = push(l, STARLARK_TOKEN_COMMA, 1);
		l->idx += 1;
		return result;
	case UTF8_SEMICOLON:
		result = push(l, STARLARK_TOKEN_SEMICOLON, 1);
		l->idx += 1;
		return result;
	case UTF8_DOT:
		result = push(l, STARLARK_TOKEN_DOT, 1);
		l->idx += 1;
		return result;
	default:
		result = push_err(
			l,
			(struct starlark_Error){
				.code = STARLARK_ERRORCODE_UNEXPECTED_SYMBOL,
				.msg = quoted_char_string(l),
				.start = l->idx + 1,
			},
			1);
		l->idx += 1;
		return result;
	}
}

static struct StateFn text(struct starlark_Lexer *l)
{
	skip_whitespace(l);

	struct StateFn result = { 0 };
	size_t len = 0;
	uint32_t c = peek_impl(l, &len);
	switch (c) {
	case UTF8_EOF:
		return (struct StateFn){ NULL };
	case UTF8_ERROR:
		result = push_err(
			l,
			(struct starlark_Error){
				.code = STARLARK_ERRORCODE_INVALID_UTF8,
				.msg = quoted_char_string(l),
				.start = l->idx + 1,
			},
			len);
		l->idx += len;
		return result;
	case UTF8_NEWLINE:
		result = push(l, STARLARK_TOKEN_NEWLINE, 1);
		l->idx += 1;
		return result;
	default:
		break;
	}

	// Floats can be written as .1 (omitting the first number)
	if (c == UTF8_DOT) {
		l->idx += 1;
		if (starlark_isdigit(peek(l))) {
			l->idx -= 1;
			return (struct StateFn){ number };
		}
		l->idx -= 1;
	}

	// Raw string or raw byte string
	if (c == (uint32_t)u8"r"[0]) {
		l->idx += 1;
		uint32_t c2 = peek(l);
		if (c2 == UTF8_SINGLEQUOTE || c2 == UTF8_DOUBLEQUOTE ||
		    expect(l, u8"b\"") || expect(l, u8"b\'")) {
			l->idx -= 1;
			return (struct StateFn){ string };
		}

		l->idx -= 1;
	}

	// Byte string or raw byte string
	if (c == (uint32_t)u8"b"[0]) {
		l->idx += 1;
		uint32_t c2 = peek(l);
		if (c2 == UTF8_SINGLEQUOTE || c2 == UTF8_DOUBLEQUOTE ||
		    expect(l, u8"r\"") || expect(l, u8"r'")) {
			l->idx -= 1;
			return (struct StateFn){ string };
		}

		l->idx -= 1;
	}

	if (starlark_isdigit(c)) {
		return (struct StateFn){ number };
	}

	if (starlark_isalpha(c) || c == UTF8_UNDERSCORE) {
		return (struct StateFn){ identifier_or_keyword };
	}

	if (c == UTF8_SINGLEQUOTE || c == UTF8_DOUBLEQUOTE) {
		return (struct StateFn){ string };
	}

	return (struct StateFn){ symbol };
}

int starlark_lex(struct starlark_Context *ctx, const char *name,
		 const size_t src_len, const uint8_t *src,
		 struct starlark_Lexer *out)
{
	assert(ctx != NULL);
	assert(out != NULL);

	if (src_len == SIZE_MAX) {
		return STARLARK_ERROR_TOOBIG;
	}

	if (!strpool_init(&ctx->strpool)) {
		ctx->err = STARLARK_ERROR_OOM;
		return STARLARK_ERROR_OOM;
	}

	ctx->src_len = src_len;
	ctx->src = (uint8_t *)src;
	int64_t handle = strpool_add(&ctx->strpool, strlen(name), name);
	if (handle < 0) {
		ctx->err = STARLARK_ERROR_OOM;
		return ctx->err;
	}
	ctx->name = handle;

	if (ctx->src_len == 0) {
		return 0;
	}

	struct StateFn state = {
		.func = text,
	};

	*out = (struct starlark_Lexer){
		.ctx = ctx,
		.idx = SIZE_MAX,
		.toks.tags = NULL,
		.toks.starts = NULL,
		.toks.ends = NULL,
		.toks_len = 0,
		.toks_cap = 0,
	};

	while (state.func != NULL) {
		state = state.func(out);
		if (ctx->err) {
			out->toks_cap = 0;
			out->toks_len = 0;
			ctx->errs_cap = 0;
			ctx->errs_len = 0;
			free(out->toks.tags);
			free(ctx->errs.codes);
			return ctx->err;
		}
	}

	return 0;
}

static const char *TokenTag_strs[] = {
	// For when an unexpected token is found.
	"STARLARK_TOKEN_ERROR",
	"STARLARK_TOKEN_INVALID_UTF8",
	// An identifier.
	"STARLARK_TOKEN_IDENT_OR_KEYWORD",
	// '(' and ')'
	"STARLARK_TOKEN_LPAREN",
	"STARLARK_TOKEN_RPAREN",
	// '[' and ']'
	"STARLARK_TOKEN_LBRACKET",
	"STARLARK_TOKEN_RBRACKET",
	// '{' and '}'
	"STARLARK_TOKEN_LBRACE",
	"STARLARK_TOKEN_RBRACE",
	// Operators
	"STARLARK_TOKEN_PLUS",
	"STARLARK_TOKEN_PLUSEQ",
	"STARLARK_TOKEN_BITAND",
	"STARLARK_TOKEN_BITANDEQ",
	"STARLARK_TOKEN_EQ",
	"STARLARK_TOKEN_NOTEQ",
	"STARLARK_TOKEN_MINUS",
	"STARLARK_TOKEN_MINUSEQ",
	"STARLARK_TOKEN_BITOR",
	"STARLARK_TOKEN_BITOREQ",
	"STARLARK_TOKEN_LESS",
	"STARLARK_TOKEN_LEQ",
	"STARLARK_TOKEN_GREATER",
	"STARLARK_TOKEN_GEQ",
	"STARLARK_TOKEN_XOR",
	"STARLARK_TOKEN_XOREQ",
	"STARLARK_TOKEN_DIV",
	"STARLARK_TOKEN_DIVINT",
	"STARLARK_TOKEN_DIVEQ",
	"STARLARK_TOKEN_LSHIFT",
	"STARLARK_TOKEN_LSHIFTEQ",
	"STARLARK_TOKEN_RSHIFT",
	"STARLARK_TOKEN_RSHIFTEQ",
	"STARLARK_TOKEN_MOD",
	"STARLARK_TOKEN_MODEQ",
	"STARLARK_TOKEN_BITNOT",
	"STARLARK_TOKEN_MUL",
	"STARLARK_TOKEN_EXP",
	"STARLARK_TOKEN_MULEQ",
	"STARLARK_TOKEN_ASSIGN",
	"STARLARK_TOKEN_COMMA",
	"STARLARK_TOKEN_SEMICOLON",
	"STARLARK_TOKEN_ELLIPSIS",
	"STARLARK_TOKEN_COLON",
	"STARLARK_TOKEN_UNDERSCORE",
	"STARLARK_TOKEN_DOT",
	// Number Literal
	"STARLARK_TOKEN_NUMBER",
	// Newline
	"STARLARK_TOKEN_NEWLINE",
	"STARLARK_TOKEN_STRING",
	"STARLARK_TOKEN_COMMENT",
};

// TODO: also print the token value.
void starlark_token_dump(FILE *f, const struct starlark_Token t)
{
	fprintf(f, "%s\n", TokenTag_strs[t.tag]);
}

void starlark_tokens_dump(struct starlark_Lexer *in, FILE *f)
{
	assert(in != NULL);
	for (size_t i = 0; i < in->toks_len; i += 1) {
		struct starlark_Token t = {
			.tag = in->toks.tags[i],
			.start = in->toks.starts[i],
		};
		starlark_token_dump(f, t);
	}
}

void starlark_Lexer_finish(struct starlark_Lexer *l)
{
	if (l == NULL) {
		return;
	}

	l->ctx = NULL;
	free(l->toks.tags);
}
