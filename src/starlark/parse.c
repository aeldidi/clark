#include <stddef.h>
#include <stdalign.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include "starlark/common.h"
#include "starlark/lex.h"
#include "starlark/parse.h"
#include "starlark/util.h"
#include "starlark/int.h"
#include "starlark/strpool.h"
#include "util/panic.h"
#include "util/fmt.h"
#include "utf8/utf8.h"

enum parse_status {
	PARSE_DONE = 0,
	PARSE_NOT_DONE,
};

extern int errno;

static bool is_hexdigit(const uint8_t c)
{
	return (c >= u8"0"[0] && c <= u8"9"[0]) ||
	       (c >= u8"a"[0] && c <= u8"f"[0]) ||
	       (c >= u8"A"[0] && c <= u8"F"[0]);
}

static uint8_t char_tohex(const uint8_t c)
{
	if (c >= u8"0"[0] && c <= u8"9"[0]) {
		return c - u8"0"[0];
	} else if (c >= u8"a"[0] && c <= u8"f"[0]) {
		return 10 + (c - u8"a"[0]);
	} else {
		return 10 + (c - u8"A"[0]);
	}
}

static void end_parse(struct starlark_Parser *p)
{
	p->idx = p->l->toks_len - 1;
}

// Returns the index of the ast node or -1 on error.
static void new_node_with_value(struct starlark_Parser *p,
				const enum starlark_AstTag tag,
				const union starlark_AstNode node)
{
	assert(p != NULL);
	if (p->ctx->err) {
		return;
	}

	if (p->ast_len + 1 >= p->ast_cap) {
		// TODO: check for overflow
		const size_t cap = (p->ast_cap + 16) * 1.5;
		size_t tags_len = cap * sizeof(p->ast.tags[0]);
		size_t idxs_len = cap * sizeof(p->ast.idxs[0]);
		size_t nodes_len = cap * sizeof(p->ast.nodes[0]);
		uint8_t *ptr = realloc(p->ast.tags,
				       tags_len + idxs_len + nodes_len +
					       alignof(enum starlark_AstTag) +
					       alignof(size_t) +
					       alignof(union starlark_AstNode));
		if (ptr == NULL) {
			p->ctx->err = STARLARK_ERROR_OOM;
			return;
		}

		p->ast.tags = (void *)ptr;
		p->ast.idxs = (void *)ALIGN_UP(ptr + tags_len, alignof(size_t));
		p->ast.nodes =
			(void *)ALIGN_UP(ptr + tags_len + idxs_len,
					 alignof(union starlark_AstNode));
		p->ast_cap = cap;
	}

	p->ast.tags[p->ast_len] = tag;
	p->ast.idxs[p->ast_len] = p->ast_len;
	p->ast.nodes[p->ast_len] = node;
	p->ast_len += 1;
}

// TODO: eventually have a different length and capacity for the nodes, since
// not every AST node has a value. Currently we allocate enough room for a node
// regardless of whether the node we're adding has a value.
static void new_node(struct starlark_Parser *in, const enum starlark_AstTag tag)
{
	new_node_with_value(in, tag, (union starlark_AstNode){ 0 });
}

// Returns the string of the token at in->idx + 1
static char *quoted_token_string(struct starlark_Parser *in)
{
	size_t i = in->idx + 1;
	int len = in->l->toks.ends[i] - in->l->toks.starts[i];
	char *result =
		format("'%.*s'", len, &in->ctx->src[in->l->toks.starts[i]]);
	if (result == NULL) {
		panic("format null");
	}

	return result;
}

static char *token_string(struct starlark_Parser *p)
{
	size_t i = p->idx + 1;
	int len = p->l->toks.ends[i] - p->l->toks.starts[i];
	char *result = format("%.*s", len, &p->ctx->src[p->l->toks.starts[i]]);

	return result;
}

static enum starlark_TokenTag peek_tag(struct starlark_Parser *in)
{
	return in->l->toks.tags[in->idx + 1];
}

static size_t peek_start(struct starlark_Parser *in)
{
	return in->l->toks.starts[in->idx + 1];
}

// returns the number of digits were valid.
size_t check_number_string_run(const char *str, const int base)
{
	size_t result = 0;
	char *octal_chars = u8"01234567";
	char *hex_chars = u8"0123456789abcdefABDCEF";
	char *binary_chars = u8"01";
	char *chars = u8"0123456789";

	if (base == 16) {
		chars = hex_chars;
	} else if (base == 8) {
		chars = octal_chars;
	} else if (base == 2) {
		chars = binary_chars;
	}

	for (size_t i = 0; i < strlen(str); i += 1, result += 1) {
		char c = str[i];
		if (c == UTF8_UNDERSCORE) {
			continue;
		}

		if (strchr(chars, c) == NULL) {
			return result;
		}
	}

	return result;
}

#define SINGLE_CHARACTER_ESCAPE(escape_char, output)                   \
	if (escape == (escape_char)) {                                 \
		str[i] = (output);                                     \
		char *ptr = &str[i + 1];                               \
		memmove(ptr, ptr + 1, (result.len - (i + 1)) - 1 + 1); \
		result.len -= 1;                                       \
		continue;                                              \
	}                                                              \
	(void)escape

// TODO: evaluate escape sequences
static struct starlark_String parse_string_escapes(struct starlark_Parser *p,
						   char *str)
{
	struct starlark_String result = {
		.ptr = str,
		.len = strlen(str),
	};

	// First decide if it's a byte string, raw string, both, or none.
	bool bytes = false;
	bool raw = false;

	char *ptr = str;
	while (starlark_isalpha(ptr[0])) {
		if (ptr[0] == u8"b"[0]) {
			bytes = true;
			memmove(ptr, ptr + 1, strlen(ptr));
			ptr += 1;
			result.len -= 1;
			continue;
		}

		if (ptr[0] == u8"r"[0]) {
			raw = true;
			memmove(ptr, ptr + 1, strlen(ptr));
			ptr += 1;
			result.len -= 1;
			continue;
		}

		panic("unrecognized string prefix: %c", ptr[0]);
	}

	// Then remove the quotes at the beginning and end.
	uint8_t quote_char = str[0];
	ptr = str;
	while (ptr[0] == quote_char) {
		memmove(ptr, ptr + 1, strlen(ptr));
		result.len -= 1;
	}

	ptr = &str[result.len - 1];
	while (ptr[0] == quote_char) {
		ptr[0] = '\0';
		result.len -= 1;
		ptr -= 1;
	}

	// Finally, evaluate escape sequences

	if (raw) {
		goto finished_escape_sequences;
	}
	for (size_t i = 0; i < result.len; i += 1) {
		if (str[i] != u8"\\"[0]) {
			continue;
		}

		uint8_t escape = result.ptr[i + 1];
		printf("escape = '%c'\n", escape);

		SINGLE_CHARACTER_ESCAPE(u8"a"[0], 0x07);
		SINGLE_CHARACTER_ESCAPE(u8"b"[0], 0x08);
		SINGLE_CHARACTER_ESCAPE(u8"f"[0], 0x0c);
		SINGLE_CHARACTER_ESCAPE(u8"n"[0], 0x0a);
		SINGLE_CHARACTER_ESCAPE(u8"r"[0], 0x0d);
		SINGLE_CHARACTER_ESCAPE(u8"t"[0], 0x09);
		SINGLE_CHARACTER_ESCAPE(u8"v"[0], 0x0b);
		SINGLE_CHARACTER_ESCAPE(u8"\\"[0], UTF8_BACKSLASH);
		SINGLE_CHARACTER_ESCAPE(u8"\""[0], UTF8_DOUBLEQUOTE);
		SINGLE_CHARACTER_ESCAPE(u8"'"[0], UTF8_SINGLEQUOTE);

		if (starlark_isdigit(escape)) {
			puts("octal escape");
			// octal escape
			uint8_t value = 0;
			size_t j = 0;
			for (; j < 3 && i + 1 + j < result.len; j += 1) {
				if (!starlark_isdigit(str[i + 1 + j])) {
					break;
				}

				if (str[i + 1 + j] >= u8"8"[0]) {
					struct starlark_Error err = {
						.code = STARLARK_ERRORCODE_INVALID_ESCAPE,
						.start = p->idx + i,
					};

					err.msg = format("'%c'", escape);
					if (err.msg == NULL ||
					    !err_append(p->ctx, err)) {
						p->ctx->err =
							STARLARK_ERROR_OOM;
						return result;
					}

					result.ptr = NULL;
					return result;
				}

				value +=
					(str[i + 1 + j] - u8"0"[0]) * pow(8, j);
			}

			str[i] = value;
			char *ptr = &str[i + 1];
			memmove(ptr, ptr + j, (result.len - (i + 1)) - j + 1);
			result.len -= j;
			continue;
		}

		if (escape == u8"x"[0]) {
			puts("hex escape");
			// hex escape
			if (i + 3 >= result.len ||
			    !is_hexdigit(result.ptr[i + 2]) ||
			    !is_hexdigit(result.ptr[i + 3])) {
				struct starlark_Error err = {
					.code = STARLARK_ERRORCODE_INVALID_ESCAPE,
					.start = p->idx + i,
				};

				err.msg = format("'%c'", escape);
				if (err.msg == NULL ||
				    !err_append(p->ctx, err)) {
					p->ctx->err = STARLARK_ERROR_OOM;
					result.ptr = NULL;
					return result;
				}

				result.ptr = NULL;
				return result;
			}

			char *endptr = NULL;
			errno = 0;
			str[i] = strtoul(&str[i + 2], &endptr, 16) & 0xff;
			if (errno != 0) {
				struct starlark_Error err = {
					.code = STARLARK_ERRORCODE_INVALID_ESCAPE,
					.start = p->idx + i,
				};

				err.msg = format("'%c'", escape);
				if (err.msg == NULL ||
				    !err_append(p->ctx, err)) {
					p->ctx->err = STARLARK_ERROR_OOM;
					result.ptr = NULL;
					return result;
				}

				result.ptr = NULL;
				return result;
			}

			char *ptr = &str[i + 1];
			memmove(ptr, ptr + 3, (result.len - (i + 1)) - 3 + 1);
			result.len -= 3;
			continue;
		}

		if (escape == u8"U"[0]) {
			puts("big u");
			// big unicode escape
			if (i + 9 >= result.len ||
			    !is_hexdigit(result.ptr[i + 2]) ||
			    !is_hexdigit(result.ptr[i + 3]) ||
			    !is_hexdigit(result.ptr[i + 4]) ||
			    !is_hexdigit(result.ptr[i + 5]) ||
			    !is_hexdigit(result.ptr[i + 6]) ||
			    !is_hexdigit(result.ptr[i + 7]) ||
			    !is_hexdigit(result.ptr[i + 8]) ||
			    !is_hexdigit(result.ptr[i + 9])) {
				puts("err");
				struct starlark_Error err = {
					.code = STARLARK_ERRORCODE_INVALID_ESCAPE,
					.start = p->idx + i,
				};

				err.msg = format(
					"unicode escape must be in the form "
					"\\uXXXX or \\UXXXXXXXX, where the Xs "
					"are a valid Unicode codepoint");
				if (err.msg == NULL ||
				    !err_append(p->ctx, err)) {
					p->ctx->err = STARLARK_ERROR_OOM;
					result.ptr = NULL;
					return result;
				}

				result.ptr = NULL;
				return result;
			}

			char *tmp = format("%.*s", 8, &str[i + 2]);
			char *endptr = NULL;
			errno = 0;
			uint32_t codepoint = strtoull(tmp, &endptr, 16);
			free(tmp);
			if (errno != 0 || !utf8_codepoint_valid(codepoint)) {
				struct starlark_Error err = {
					.code = STARLARK_ERRORCODE_INVALID_ESCAPE,
					.start = p->idx + i,
				};

				err.msg = format(
					"unicode escape must be in the form "
					"\\uXXXX or \\UXXXXXXXX, where the Xs "
					"are a valid Unicode codepoint");
				if (err.msg == NULL ||
				    !err_append(p->ctx, err)) {
					p->ctx->err = STARLARK_ERROR_OOM;
					result.ptr = NULL;
					return result;
				}

				result.ptr = NULL;
				return result;
			}

			utf8_codepoint_encode(codepoint, result.len - i,
					      (uint8_t *)&str[i]);

			size_t len = utf8_codepoint_size(codepoint);
			size_t size_to_remove = 10 - len;
			char *ptr = &str[i + len + 1];
			memmove(ptr, ptr + size_to_remove,
				(result.len - (i + 1)) - size_to_remove + 1);
			result.len -= size_to_remove;
			printf("next: %c\n", str[i]);
			continue;
		}

		if (escape == u8"u"[0]) {
			puts("small u");
			// small unicode escape
			if (i + 5 >= result.len ||
			    !is_hexdigit(result.ptr[i + 2]) ||
			    !is_hexdigit(result.ptr[i + 3]) ||
			    !is_hexdigit(result.ptr[i + 4]) ||
			    !is_hexdigit(result.ptr[i + 5])) {
				struct starlark_Error err = {
					.code = STARLARK_ERRORCODE_INVALID_ESCAPE,
					.start = p->idx + i,
				};

				err.msg = format(
					"unicode escape must be in the form "
					"\\uXXXX or \\UXXXXXXXX, where the Xs "
					"are a valid Unicode codepoint");
				if (err.msg == NULL ||
				    !err_append(p->ctx, err)) {
					p->ctx->err = STARLARK_ERROR_OOM;
					result.ptr = NULL;
					return result;
				}

				result.ptr = NULL;
				return result;
			}

			char *tmp = format("%.*s", 4, &str[i + 2]);
			char *endptr = NULL;
			errno = 0;
			uint32_t codepoint = strtoull(tmp, &endptr, 16);
			free(tmp);
			if (errno != 0 || !utf8_codepoint_valid(codepoint)) {
				struct starlark_Error err = {
					.code = STARLARK_ERRORCODE_INVALID_ESCAPE,
					.start = p->idx + i,
				};

				err.msg = format(
					"unicode escape must be in the form "
					"\\uXXXX or \\UXXXXXXXX, where the Xs "
					"are a valid Unicode codepoint");
				if (err.msg == NULL ||
				    !err_append(p->ctx, err)) {
					p->ctx->err = STARLARK_ERROR_OOM;
					result.ptr = NULL;
					return result;
				}

				result.ptr = NULL;
				return result;
			}

			utf8_codepoint_encode(codepoint, result.len - i,
					      (uint8_t *)&str[i]);

			size_t len = utf8_codepoint_size(codepoint);
			size_t size_to_remove = 6 - len;
			char *ptr = &str[i + len + 1];
			memmove(ptr, ptr + size_to_remove,
				(result.len - (i + 1)) - size_to_remove + 1);
			result.len -= size_to_remove;
			continue;
		}

		if (escape == UTF8_NEWLINE) {
			// escaped newline
			char *ptr = &str[i];
			memmove(ptr, ptr + 2, (result.len - (i + 1)) - 2 + 1);
			result.len -= 2;
			i -= 1;
			continue;
		}

		struct starlark_Error err = {
			.code = STARLARK_ERRORCODE_INVALID_ESCAPE,
			.start = p->idx + i,
		};

		err.msg = format("'%c'", escape);
		if (err.msg == NULL || !err_append(p->ctx, err)) {
			p->ctx->err = STARLARK_ERROR_OOM;
			result.ptr = NULL;
			return result;
		}

		result.ptr = NULL;
		return result;
	}
finished_escape_sequences:

	return result;
}

static double parse_float(struct starlark_Parser *in, char *str)
{
	int old_errno = errno;
	char *ptr = str;

	// strtod also accepts hexadecimal numbers with exponents containing p.
	// We don't want this, so if it manages to get in here, just panic.
	if (strpbrk(str, u8"pPxX") != NULL) {
		panic("A hexadecimal float managed to get into parse_float: %s",
		      str);
	}

	errno = 0;
	double result = strtod(str, &ptr);
	if (errno != 0) {
		enum starlark_ErrorCode code = STARLARK_ERRORCODE_FLOAT_INVALID;
		if (errno == ERANGE) {
			code = STARLARK_ERRORCODE_FLOAT_TOO_BIG;
		}

		struct starlark_Error err = {
			.code = code,
			.msg = token_string(in),
			.start = in->l->toks.starts[in->idx],
		};

		if (!err_append(in->ctx, err)) {
			in->ctx->err = STARLARK_ERROR_OOM;
		}

		return 0;
	}
	errno = old_errno;

	return result;
}

static struct starlark_Int *parse_int(struct starlark_Parser *in, char *str)
{
	char *ptr = str;

	int base = 10;

	if (strncmp(ptr, u8"0x", 2) == 0 || strncmp(ptr, u8"0x", 2) == 0) {
		base = 16;
		str += 2;
		ptr = str;
	} else if (strncmp(ptr, u8"0b", 2) == 0 ||
		   strncmp(ptr, u8"0B", 2) == 0) {
		base = 2;
		str += 2;
		ptr = str;
	} else if (strncmp(ptr, u8"0o", 2) == 0 ||
		   strncmp(ptr, u8"0O", 2) == 0) {
		base = 8;
		str += 2;
		ptr = str;
	}

	ptr += check_number_string_run(ptr, base);
	if (ptr[0] == u8"e"[0] || ptr[0] == u8"E"[0]) {
		panic("exponent in parse_int: I thought I checked for this "
		      "in parse_operand");
	}

	struct starlark_Int *result = Int_from_str(str, base);
	if (result == NULL) {
		in->ctx->err = STARLARK_ERROR_OOM;
		return NULL;
	}

	return result;
}

// Operand = identifier
//         | int | float | string | bytes
//         | ListExpr | ListComp
//         | DictExpr | DictComp
//         | '(' [Expression [',']] ')'
//         .
// returns true
static void parse_operand(struct starlark_Parser *in)
{
	if (in->ctx->err != 0) {
		return;
	}

	union starlark_AstNode node = { 0 };
	switch (peek_tag(in)) {
	case STARLARK_TOKEN_IDENT_OR_KEYWORD:
		node.as_identifier.str = token_string(in);
		new_node_with_value(in, STARLARK_NODE_IDENTIFIER, node);
		in->idx += 1;
		break;
	case STARLARK_TOKEN_NUMBER: {
		char *str = token_string(in);
		if (strchr(str, u8"."[0]) != NULL ||
		    ((strncmp(str, u8"0x", 2) != 0 &&
		      strncmp(str, u8"0X", 2) != 0) &&
		     strpbrk(str, u8"eE") != NULL)) {
			node.as_float = parse_float(in, str);
			new_node_with_value(in, STARLARK_NODE_FLOAT, node);
			free(str);
			in->idx += 1;
			break;
		}

		node.as_int = parse_int(in, str);
		if (node.as_int == NULL) {
			new_node(in, STARLARK_NODE_ERROR);
			free(str);
			in->idx += 1;
			return;
		}
		new_node_with_value(in, STARLARK_NODE_INT, node);
		free(str);
		in->idx += 1;
		break;
	}
	case STARLARK_TOKEN_STRING: {
		char *tmp = token_string(in);
		node.as_str = parse_string_escapes(in, tmp);
		if (node.as_str.ptr == NULL) {
			free(tmp);
			new_node(in, STARLARK_NODE_ERROR);
			in->idx += 1;
			return;
		}

		new_node_with_value(in, STARLARK_NODE_STRING, node);
		in->idx += 1;
		break;
	}
	default:
		unimplemented();
	}
}

static enum parse_status status(struct starlark_Parser *p)
{
	if (p->ctx->err != 0) {
		return PARSE_DONE;
	}

	if (p->idx + 1 >= p->l->toks_len) {
		return PARSE_DONE;
	}

	return PARSE_NOT_DONE;
}

static void parse_file(struct starlark_Parser *p)
{
	if (p->ctx->err != 0) {
		return;
	}

	while (status(p) != PARSE_DONE) {
		switch (peek_tag(p)) {
		case STARLARK_TOKEN_NEWLINE:
			p->idx += 1;
			break;
		default:
			parse_operand(p);
		}
	}
}

int starlark_parse_tokens(struct starlark_Context *ctx,
			  struct starlark_Lexer *l, struct starlark_Parser *out)
{
	assert(ctx != NULL);
	assert(l != NULL);
	assert(out != NULL);

	if (l->toks_len == 0) {
		*out = (struct starlark_Parser){
			.ctx = ctx,
			.l = l,
		};
		return 0;
	}

	struct starlark_Parser p = {
		.ctx = ctx,
		.l = l,
		.idx = SIZE_MAX,
	};

	parse_file(&p);

	if (ctx->err != 0) {
		out->l = NULL;
		starlark_Parser_finish(&p);
		return ctx->err;
	}

	*out = p;
	return 0;
}

int starlark_parse(struct starlark_Context *ctx, const char *name,
		   const size_t src_len, const uint8_t *src,
		   struct starlark_Parser *out)
{
	assert(ctx != NULL);
	assert(out != NULL);

	struct starlark_Lexer l = { 0 };
	int ret = starlark_lex(ctx, name, src_len, src, &l);

	if (ret != 0) {
		return ret;
	}

	ret = starlark_parse_tokens(ctx, &l, out);
	starlark_Lexer_finish(&l);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

void starlark_node_dump(struct starlark_Parser *in,
			const struct starlark_Node n, FILE *f)
{
	switch (n.tag) {
	case STARLARK_NODE_ERROR:
		fprintf(f, "ERROR\n");
		break;
	case STARLARK_NODE_IDENTIFIER:
		fprintf(f, "IDENTIFIER: %s\n",
			in->ast.nodes[n.idx].as_identifier.str);
		break;
	case STARLARK_NODE_OPERAND:
		fprintf(f, "OPERAND:    ");
		break;
	case STARLARK_NODE_INT: {
		char *str = Int_to_str(in->ast.nodes[n.idx].as_int, 10);
		if (str == NULL) {
			fprintf(f,
				"INT:        (error retrieving INT value)\n");
		} else {
			fprintf(f, "INT:        %s\n", str);
		}
		free(str);
		break;
	}
	case STARLARK_NODE_FLOAT:
		fprintf(f, "FLOAT:      %g\n", in->ast.nodes[n.idx].as_float);
		break;
	case STARLARK_NODE_STRING:
		fprintf(f, "STRING:     '%.*s'\n",
			(int)in->ast.nodes[in->idx].as_str.len,
			in->ast.nodes[n.idx].as_str.ptr);
		break;
	default:
		fprintf(f, "UNKNOWN TAG: (node type %u)\n", n.tag);
		break;
	}
}

void starlark_Parser_finish(struct starlark_Parser *in)
{
	if (in == NULL) {
		return;
	}

	for (size_t i = 0; i < in->ast_len; i += 1) {
		switch (in->ast.tags[i]) {
		case STARLARK_NODE_ERROR:
			break;
		case STARLARK_NODE_IDENTIFIER:
			free(in->ast.nodes[in->ast.idxs[i]].as_identifier.str);
			break;
		case STARLARK_NODE_FLOAT:
			break;
		case STARLARK_NODE_INT:
			Int_destroy(in->ast.nodes[in->ast.idxs[i]].as_int);
			break;
		case STARLARK_NODE_STRING:
			free(in->ast.nodes[in->ast.idxs[i]].as_str.ptr);
			break;
		default: {
			struct starlark_Node n = {
				.idx = in->ast.idxs[i],
				.tag = in->ast.tags[i],
			};
			fprintf(stderr, "don't know how to free node type!\n");
			starlark_node_dump(in, n, stderr);
			unimplemented();
		}
		}
	}

	free(in->ast.tags);
}

void starlark_ast_dump(struct starlark_Parser *in, FILE *f)
{
	// TODO: traverse AST instead of doing this
	for (size_t i = 0; i < in->ast_len; i += 1) {
		struct starlark_Node n = {
			.idx = in->ast.idxs[i],
			.tag = in->ast.tags[i],
		};

		starlark_node_dump(in, n, f);
	}
}
