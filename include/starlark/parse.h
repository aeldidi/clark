#ifndef STARLARK_PARSE_H
#define STARLARK_PARSE_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "starlark/common.h"
#include "starlark/lex.h"

// All the ast nodes.
enum starlark_AstTag {
	STARLARK_NODE_ERROR = 0,
	STARLARK_NODE_IDENTIFIER,
	STARLARK_NODE_INT,
	STARLARK_NODE_FLOAT,
	STARLARK_NODE_STRING,
	STARLARK_NODE_OPERAND,
};

// A single ast node.
struct starlark_Node {
	uint32_t idx;
	enum starlark_AstTag tag;
};

struct starlark_String {
	size_t len;
	char *ptr;
};

union starlark_AstNode {
	struct {
		// This will eventually be a handle into a strpool.
		char *str;
	} as_identifier;
	struct starlark_String as_str;
	struct starlark_Int *as_int;
	double as_float;
};

struct starlark_Parser {
	struct starlark_Context *ctx;
	struct starlark_Lexer *l;
	size_t idx;

	size_t ast_len;
	size_t ast_cap;
	struct {
		enum starlark_AstTag *tags;
		size_t *idxs;
		union starlark_AstNode *nodes;
	} ast;
};

// Parses the tokens from a call to starlark_lex into the parser out.
STARLARK_PUBLIC
int starlark_parse_tokens(struct starlark_Context *ctx,
			  struct starlark_Lexer *l,
			  struct starlark_Parser *out);

// Parses the file given into the parser out.
STARLARK_PUBLIC
int starlark_parse(struct starlark_Context *ctx, const char *name,
		   const size_t src_len, const uint8_t *src,
		   struct starlark_Parser *out);

STARLARK_PUBLIC
void starlark_node_dump(struct starlark_Parser *in,
			const struct starlark_Node n, FILE *f);

STARLARK_PUBLIC
void starlark_ast_dump(struct starlark_Parser *in, FILE *f);

// Disposes of the parser and its associated Lexer.
STARLARK_PUBLIC
void starlark_Parser_finish(struct starlark_Parser *in);

#endif // STARLARK_PARSE_H
