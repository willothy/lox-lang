#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "object.h"
#include "scanner.h"
#include "chunk.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct Parser {
	Token current;
	Token previous;
	bool had_error;
	bool panic_mode;
} Parser;

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
} Precedence;

typedef void (*ParseFn)(bool can_assign);

typedef struct  {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

Parser parser;
Chunk *compiling_chunk;

static Chunk *current_chunk() {
	return compiling_chunk;
}

static void error_at(Token *token, const char* message) {
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

static void consume(TokenType type, const char* message) {
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

static uint32_t emit_constant(Value value) {
	return chunk_write_constant(current_chunk(), value, parser.previous.line);
}

static void end_compilation() {
	emit_return();
	#ifdef DEBUG_PRINT_CODE
	if (!parser.had_error) {
		disassemble_chunk(current_chunk(), "code");
	}
	#endif
	// compiling_chunk = NULL;
}

static bool check(TokenType type) {
	return parser.current.type == type;
}

static bool match(TokenType type) {
	if (!check(type)) return false;
	advance();
	return true;
}

static void expression();
static void binary(bool can_assign);
static void statement();
static void declaration();
static ParseRule *get_rule(TokenType type);

static void number(bool can_assign) {
	double value = strtod(parser.previous.start, NULL);
	emit_constant(NUMBER_VAL(value));
}

static void parse_precedence(Precedence precedence) {
	advance();
	ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
	if (prefix_rule == NULL) {
		error("Expect expression.");
		return;
	}

	bool can_assign = precedence <= PREC_ASSIGNMENT;
	prefix_rule(can_assign);

	while (precedence <= get_rule(parser.current.type)->precedence) {
		advance();
		ParseFn infix_rule = get_rule(parser.previous.type)->infix;
		infix_rule(can_assign);
	}

	if (can_assign && match(TOKEN_EQUAL)) {
		error("Invalid assignment target.");
	}
}

static void expression() {
	parse_precedence(PREC_ASSIGNMENT);
}

static void expression_statement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
	emit_byte(OP_POP);
}

static void print_statement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emit_byte(OP_PRINT);
}

static void synchronize() {
	parser.panic_mode = false;

	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON) return;
		switch (parser.current.type) {
		case TOKEN_CLASS:
		case TOKEN_FUN:
		case TOKEN_VAR:
		case TOKEN_FOR:
		case TOKEN_IF:
		case TOKEN_WHILE:
		case TOKEN_PRINT:
		case TOKEN_RETURN:
			return;
		default:
			;
		}

		advance();
	}
}

static void define_global(uint32_t global) {
	if (global > UINT8_MAX) {
		// TODO: is this the correct order?
		emit_bytes(OP_DEFINE_GLOBAL_LONG, global & 0xff);
		emit_bytes( (global >> 8) & 0xff, (global >> 16) & 0xff);
	} else {
		emit_bytes(OP_DEFINE_GLOBAL, global & 0xff);
	}
}

static uint32_t identifier_constant(Token *name) {
	Chunk *chunk = current_chunk();
	ObjectString *ident = ref_string((char*)name->start, name->length);

	// slower at compile time, but saves memory by deduplicating constant idents
	for (uint32_t i = 0; i < chunk->constants.count; i++) {
		Value value = chunk->constants.values[i];
		if (IS_STRING(value) && AS_STRING(value)->hash == ident->hash) {
			// There is a string with the same hash, so we can
			// just return its index.
			return i;
		}
	}
	return chunk_add_constant(current_chunk(), OBJ_VAL(ident));
	// return chunk_add_constant(current_chunk(), OBJ_VAL(ref_string((char*)name->start, name->length)));
}

static uint32_t parse_variable(const char *message) {
	consume(TOKEN_IDENTIFIER, message);
	return identifier_constant(&parser.previous);
}

static void var_declaration() {
	uint32_t global = parse_variable("Expect variable name.");

	if (match(TOKEN_EQUAL)) {
		expression();
	} else {
		emit_byte(OP_NIL);
	}
	consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

	define_global(global);
}

static void declaration() {
	if (match(TOKEN_VAR)) {
		var_declaration();
	} else {
		statement();
	}

	if (parser.panic_mode) synchronize();
}

static void statement() {
	if (match(TOKEN_PRINT)) {
		print_statement();
	} else {
		expression_statement();
	}
}

static void grouping(bool can_assign) {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(bool can_assign) {
	TokenType operator_type = parser.previous.type;

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

static void literal(bool can_assign) {
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

static void string(bool can_assign) {
	ObjectString *str = ref_string((char*)parser.previous.start + 1, parser.previous.length-2);
	emit_constant(OBJ_VAL(str));
}

static void named_variable(Token name, bool can_assign) {
	uint32_t arg = identifier_constant(&name);
	bool set = false;
	if (can_assign && match(TOKEN_EQUAL)) {
		expression();
		set = true;
	}
	if (arg > UINT8_MAX) {
		emit_bytes(set ? OP_SET_GLOBAL_LONG : OP_GET_GLOBAL_LONG, arg & 0xff);
		emit_bytes( (arg >> 8) & 0xff, (arg >> 16) & 0xff);
	} else {
		emit_bytes(set ? OP_SET_GLOBAL : OP_GET_GLOBAL, (uint8_t)arg);
	}
}

static void variable(bool can_assign) {
	named_variable(parser.previous, can_assign);
}

ParseRule rules[] = {
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
	[TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
	[TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
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

static ParseRule *get_rule(TokenType type) {
	return &rules[type];
}

static void binary(bool can_assign) {
	TokenType operator_type = parser.previous.type;
	ParseRule *rule = get_rule(operator_type);
	parse_precedence((Precedence)(rule->precedence + 1)); // right operand

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


bool compile(const char *src, Chunk *chunk) {
	scanner_init(src);

	parser.had_error = false;
	parser.panic_mode = false;

	compiling_chunk = chunk;

	advance();

	while (!match(TOKEN_EOF)) {
		declaration();
	}

	end_compilation();

	return !parser.had_error;
}
