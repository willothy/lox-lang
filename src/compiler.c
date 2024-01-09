#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "chunk.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct parser_t {
	token_t current;
	token_t previous;
	bool had_error;
	bool panic_mode;
} parser_t;

typedef enum {
	PREC_NONE,
	PREC_ASSIGNMENT, // =
	PREC_OR,    // or
	PREC_AND,   // and
	PREC_EQUALITY, // == !=
	PREC_COMPARISON, // < > <= >=
	PREC_TERM,  // + -
	PREC_FACTOR, // * /
	PREC_UNARY, // ! -
	PREC_CALL,  // . ()
	PREC_PRIMARY
} precedence_t;

typedef void (*parse_fn_t)();

typedef struct  {
	parse_fn_t prefix;
	parse_fn_t infix;
	precedence_t precedence;
} parse_rule_t;

parser_t parser;
chunk_t *compiling_chunk;

static chunk_t *current_chunk() {
	return compiling_chunk;
}

static void error_at(token_t *token, const char* message) {
	if (parser.panic_mode) return;
	parser.panic_mode = true;
	fprintf(stderr, "[line %zu] Error", token->line);

	switch(token->type) {
	case TOKEN_EOF:
		fprintf(stderr, " at end");
		break;
	case TOKEN_ERROR:
		break;
	default:
		fprintf(stderr, " at '%.*s'", (int)token->length, token->start);
		break;
	}
	fprintf(stderr, ": %s\n", message);
	parser.had_error = true;
}

static void error_at_current(const char *message) {
	error_at(&parser.current, message);
}

static void error(const char* message) {
	error_at(&parser.previous, message);
}

static void advance() {
	parser.previous = parser.current;

	for (;;) {
		parser.current = scanner_next_token();
		if (parser.current.type != TOKEN_ERROR) break;
		error_at_current(parser.current.start);
	}
}

static void consume(token_type_t type, const char* message) {
	if (parser.current.type == type) {
		advance();
		return;
	}

	error_at_current(message);
}

static void emit_byte(uint8_t byte) {
	chunk_write(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint32_t byte1, uint32_t byte2) {
	emit_byte(byte1);
	emit_byte(byte2);
}

static void emit_return() {
	emit_byte(OP_RETURN);
}

static void emit_constant(double value) {
	chunk_write_constant(current_chunk(), NUMBER_VAL(value), parser.previous.line);
}

static void end_compilation() {
	emit_return();
	#ifdef DEBUG_PRINT_CODE
	if (!parser.had_error) {
		disassemble_chunk(current_chunk(), "code");
	}
	#endif
	compiling_chunk = NULL;
}

static void expression();
static void binary();
static parse_rule_t *get_rule(token_type_t type);
// static void parse_precedence(precedence_t precedence);

static void number() {
	double value = strtod(parser.previous.start, NULL);
	emit_constant(value);
}

static void parse_precedence(precedence_t precedence) {
	advance();
	parse_fn_t prefix_rule = get_rule(parser.previous.type)->prefix;
	if (prefix_rule == NULL) {
		error("Expect expression.");
		return;
	}

	prefix_rule();

	while (precedence <= get_rule(parser.current.type)->precedence) {
		advance();
		parse_fn_t infix_rule = get_rule(parser.previous.type)->infix;
		infix_rule();
	}
}

static void expression() {
	parse_precedence(PREC_ASSIGNMENT);
}

static void grouping() {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary() {
	token_type_t operator_type = parser.previous.type;

	parse_precedence(PREC_UNARY); // operand

	switch (operator_type) {
	case TOKEN_MINUS: {
		emit_byte(OP_NEGATE);
		break;
	}
	case TOKEN_BANG: {
		emit_byte(OP_NOT);
		break;
	}
	default: return; // unreachable
	}
}

static void literal() {
	switch (parser.previous.type) {
	case TOKEN_FALSE: {
		emit_byte(OP_FALSE);
		break;
	}
	case TOKEN_NIL: {
		emit_byte(OP_NIL);
		break;
	}
	case TOKEN_TRUE: {
		emit_byte(OP_TRUE);
		break;
	}
	default: return; // unreachable
	}
}

parse_rule_t rules[] = {
	[TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
	[TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
	[TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
	[TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
	[TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
	[TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
	[TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
	[TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
	[TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
	[TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
	[TOKEN_BANG_EQUAL]    = {NULL,     binary,   PREC_NONE},
	[TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_EQUAL_EQUAL]   = {NULL,     binary,   PREC_NONE},
	[TOKEN_GREATER]       = {NULL,     binary,   PREC_NONE},
	[TOKEN_GREATER_EQUAL] = {NULL,     binary,   PREC_NONE},
	[TOKEN_LESS]          = {NULL,     binary,   PREC_NONE},
	[TOKEN_LESS_EQUAL]    = {NULL,     binary,   PREC_NONE},
	[TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
	[TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
	[TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
	[TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
	[TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
	[TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
	[TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
	[TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
	[TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
	[TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
	[TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static parse_rule_t *get_rule(token_type_t type) {
	return &rules[type];
}

static void binary() {
	token_type_t operator_type = parser.previous.type;
	parse_rule_t *rule = get_rule(operator_type);
	parse_precedence((precedence_t)(rule->precedence + 1)); // right operand

	switch (operator_type) {
	case TOKEN_BANG_EQUAL: emit_bytes(OP_EQUAL, OP_NOT); break;
	case TOKEN_EQUAL_EQUAL: emit_byte(OP_EQUAL); break;
	case TOKEN_GREATER: emit_byte(OP_GREATER); break;
	case TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
	case TOKEN_LESS: emit_byte(OP_LESS); break;
	case TOKEN_LESS_EQUAL: emit_bytes(OP_GREATER, OP_NOT); break;
	case TOKEN_PLUS: emit_byte(OP_ADD); break;
	case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
	case TOKEN_STAR: emit_byte(OP_MULTIPLY); break;
	case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
	default: return; // unreachable
	}
}

bool compile(const char *src, chunk_t *chunk) {
	scanner_init(src);

	parser.had_error = false;
	parser.panic_mode = false;

	compiling_chunk = chunk;

	advance();
	expression();
	consume(TOKEN_EOF, "Expect end of expression.");

	end_compilation();

	return !parser.had_error;
}
