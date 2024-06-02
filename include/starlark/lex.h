#ifndef STARLARK_LEX_H
#define STARLARK_LEX_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "starlark/common.h"

enum starlark_TokenTag {
	// For when an unexpected token is found.
	STARLARK_TOKEN_ERROR = 0,
	STARLARK_TOKEN_INVALID_UTF8,
	// An identifier.
	STARLARK_TOKEN_IDENT_OR_KEYWORD,
	// '(' and ')'
	STARLARK_TOKEN_LPAREN,
	STARLARK_TOKEN_RPAREN,
	// '[' and ']'
	STARLARK_TOKEN_LBRACKET,
	STARLARK_TOKEN_RBRACKET,
	// '{' and '}'
	STARLARK_TOKEN_LBRACE,
	STARLARK_TOKEN_RBRACE,
	// Operators
	STARLARK_TOKEN_PLUS,
	STARLARK_TOKEN_PLUSEQ,
	STARLARK_TOKEN_BITAND,
	STARLARK_TOKEN_BITANDEQ,
	STARLARK_TOKEN_EQ,
	STARLARK_TOKEN_NOTEQ,
	STARLARK_TOKEN_MINUS,
	STARLARK_TOKEN_MINUSEQ,
	STARLARK_TOKEN_BITOR,
	STARLARK_TOKEN_BITOREQ,
	STARLARK_TOKEN_LESS,
	STARLARK_TOKEN_LEQ,
	STARLARK_TOKEN_GREATER,
	STARLARK_TOKEN_GEQ,
	STARLARK_TOKEN_XOR,
	STARLARK_TOKEN_XOREQ,
	STARLARK_TOKEN_DIV,
	STARLARK_TOKEN_DIVINT,
	STARLARK_TOKEN_DIVEQ,
	STARLARK_TOKEN_LSHIFT,
	STARLARK_TOKEN_LSHIFTEQ,
	STARLARK_TOKEN_RSHIFT,
	STARLARK_TOKEN_RSHIFTEQ,
	STARLARK_TOKEN_MOD,
	STARLARK_TOKEN_MODEQ,
	STARLARK_TOKEN_BITNOT,
	STARLARK_TOKEN_MUL,
	STARLARK_TOKEN_EXP,
	STARLARK_TOKEN_MULEQ,
	STARLARK_TOKEN_ASSIGN,
	STARLARK_TOKEN_COMMA,
	STARLARK_TOKEN_SEMICOLON,
	STARLARK_TOKEN_ELLIPSIS,
	STARLARK_TOKEN_COLON,
	STARLARK_TOKEN_UNDERSCORE,
	STARLARK_TOKEN_DOT,
	// Number Literal
	STARLARK_TOKEN_NUMBER,
	// Newline
	STARLARK_TOKEN_NEWLINE,
	STARLARK_TOKEN_STRING,
	STARLARK_TOKEN_COMMENT,
};

// Everything not included in this struct can be computed lazily.
struct starlark_Token {
	enum starlark_TokenTag tag;
	size_t start;
	size_t end;
};

struct starlark_Lexer {
	struct starlark_Context *ctx;
	size_t idx;

	size_t toks_len;
	size_t toks_cap;
	struct {
		enum starlark_TokenTag *tags;
		size_t *starts;
		size_t *ends;
	} toks;
};

STARLARK_PUBLIC
int starlark_lex(struct starlark_Context *ctx, const char *name,
		 const size_t src_len, const uint8_t *src,
		 struct starlark_Lexer *out);

STARLARK_PUBLIC
void starlark_token_dump(FILE *f, const struct starlark_Token t);

STARLARK_PUBLIC
void starlark_tokens_dump(struct starlark_Lexer *ctx, FILE *f);

STARLARK_PUBLIC
void starlark_Lexer_finish(struct starlark_Lexer *l);

#endif // STARLARK_LEX_H
