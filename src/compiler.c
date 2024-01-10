#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
	Token name;
	uint32_t depth;
} Local;

typedef enum {
	FN_TYPE_FUNCTION,
	FN_TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
	struct Compiler *enclosing;

	ObjectFunction *function;
	FunctionType type;

	Local locals[UINT8_COUNT];
	uint32_t local_count;
	uint32_t scope_depth;
} Compiler;

Parser parser;
Compiler *current = NULL;

static Chunk *current_chunk() {
	return &current->function->chunk;
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

static void emit_loop(uint32_t loop_start) {
	emit_byte(OP_LOOP);
	uint32_t offset = current_chunk()->count - loop_start + 4;
	if (offset > UINT32_MAX) {
		error("Loop body too large.");
	}
	emit_byte((offset >> 24) & 0xff);
	emit_byte((offset >> 16) & 0xff);
	emit_byte((offset >> 8) & 0xff);
	emit_byte(offset & 0xff);
}

static uint32_t emit_jump(uint8_t instruction) {
	emit_byte(instruction);
	// The source implementation uses a 16-bit offset, but I'm using 32 bits.
	emit_byte(0xff);
	emit_byte(0xff);
	emit_byte(0xff);
	emit_byte(0xff);
	return current_chunk()->count - 4;
}

static void patch_jump(uint32_t offset) {
	Chunk *chunk = current_chunk();
	uint32_t jump = chunk->count - offset - 4;

	if (jump > UINT32_MAX) {
		error("Too much code to jump over.");
	}

	chunk->code[offset]     = (jump >> 24) & 0xff;
	chunk->code[offset + 1] = (jump >> 16) & 0xff;
	chunk->code[offset + 2] = (jump >> 8) & 0xff;
	chunk->code[offset + 3] = jump & 0xff;
}

static void emit_return() {
	emit_byte(OP_RETURN);
}

static uint32_t emit_constant(Value value) {
	return chunk_write_constant(current_chunk(), value, parser.previous.line);
}

static void compiler_init(Compiler *compiler, FunctionType type) {
	compiler->enclosing = current;
	compiler->type = type;
	compiler->local_count = 0;
	compiler->scope_depth = 0;
	compiler->function = function_new();
	current = compiler;

	if (type  != FN_TYPE_SCRIPT) {
		current->function->name = copy_string((char*)parser.previous.start, parser.previous.length);
	}

	Local *local = &current->locals[current->local_count++];
	local->depth = 0;
	local->name.start = "";
	local->name.length = 0;
}

static ObjectFunction *end_compilation() {
	emit_return();
	ObjectFunction *function = current->function;

	#ifdef DEBUG_PRINT_CODE
	if (!parser.had_error) {
		disassemble_chunk(current_chunk(), function->name != NULL ? function->name->chars : "<script>");
	}
	#endif

	current = current->enclosing;

	return function;
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
static void block();
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

static void if_statement() {
	// TODO: The source implementation requires parentheses around the condition.
	// My version of Lox will not.
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	// TODO: support nil-matching / := / if let expr
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	uint32_t then_jump = emit_jump(OP_JUMP_IF_FALSE);
	emit_byte(OP_POP);
	// TODO: support expression result from if statements in expr position
	statement();

	uint32_t else_jump = emit_jump(OP_JUMP);

	patch_jump(then_jump);

	emit_byte(OP_POP);

	if (match(TOKEN_ELSE)) {
		statement();
	}

	patch_jump(else_jump);
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

static void add_local(Token name) {
	// TODO: support more than 256 locals
	if (current->local_count == UINT8_COUNT) {
		error("Too many local variables in function.");
		return;
	}
	Local *local = &current->locals[current->local_count++];
	local->name = name;
	local->depth = -1;
	// local->depth = current->scope_depth;
}

static void mark_initialized() {
	if (current->scope_depth == 0) {
		return;
	}
	current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void define_variable(uint32_t global) {
	if (current->scope_depth > 0) {
		mark_initialized();
		return;
	};

	if (global > UINT8_MAX) {
		// TODO: is this the correct order?
		emit_bytes(OP_DEFINE_GLOBAL_LONG, global & 0xff);
		emit_bytes( (global >> 8) & 0xff, (global >> 16) & 0xff);
	} else {
		emit_bytes(OP_DEFINE_GLOBAL, global & 0xff);
	}
}

static void declare_variable() {
	if (current->scope_depth == 0) return;

	Token *name = &parser.previous;

#ifndef ALLOW_SHADOWING
	for (int i = current->local_count - 1; i >= 0; i--) {
		Local* local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scope_depth) {
			break;
		}

		if (identifiers_equal(name, &local->name)) {
			error("Already a variable with this name in this scope.");
		}
	}
#endif

	add_local(*name);
}

static void begin_scope() {
	current->scope_depth++;
}

static void end_scope() {
	current->scope_depth--;

	// Drop any locals that were declared in the scope that just ended.
	// TODO: this is O(n) in the number of locals in the scope,
	// but could be O(1) if I kept track of the number of locals
	// at the start of the scope and add a SET_TOP or similar instruction.
	while (current->local_count > 0 &&
	       current->locals[current->local_count - 1].depth >
	       current->scope_depth) {
		emit_byte(OP_POP);
		current->local_count--;
	}
}

static uint32_t parse_variable(const char *message) {
	consume(TOKEN_IDENTIFIER, message);

	declare_variable();
	if (current->scope_depth > 0) return 0;

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

	define_variable(global);
}


static void function(FunctionType type) {
	Compiler compiler;
	compiler_init(&compiler, type);
	begin_scope();

	// Compile the parameter list.
	consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > UINT8_MAX) {
				error_at_current("Cannot have more than 255 parameters.");
			}
			uint32_t param_constant = parse_variable("Expect parameter name.");
			define_variable(param_constant);
		} while(match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after function parameters.");

	consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	block();

	ObjectFunction *function = end_compilation();
	emit_bytes(OP_CONSTANT, emit_constant(OBJ_VAL(function)));
}

static void fun_declaration() {
	uint32_t global = parse_variable("Expect function name.");
	mark_initialized();
	function(FN_TYPE_FUNCTION);
	define_variable(global);
}

static void declaration() {
	if (match(TOKEN_FUN)) {
		fun_declaration();
	} else if (match(TOKEN_VAR)) {
		var_declaration();
	} else {
		statement();
	}

	if (parser.panic_mode) synchronize();
}

static void block() {
	while (!check(TOKEN_RIGHT_BRACE) &&!check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void while_statement() {
	size_t loop_start = current_chunk()->count;

	// TODO: Don't require parentheses around the condition.
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	uint32_t exit_jump = emit_jump(OP_JUMP_IF_FALSE);
	emit_byte(OP_POP);
	statement();
	emit_loop(loop_start);

	patch_jump(exit_jump);
	emit_byte(OP_POP);
}

// TODO: Better numeric for syntax, for in syntax with ranges.
// TODO: Support break and continue.
//        - Support labeled blocks.
//        - Support break with value.
// TODO: Support match statements / exprs.
// TODO: Support conditionless loops in expression position.
static void for_statement() {
	begin_scope();
	// TODO: Don't require parentheses.
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
	if (match(TOKEN_SEMICOLON)) {
		// No initializer.
	} else if (match(TOKEN_VAR)) {
		var_declaration();
	} else {
		expression_statement();
	}

	uint32_t loop_start = current_chunk()->count;

	uint32_t exit_jump = 0;
	bool has_condition = !match(TOKEN_SEMICOLON);
	if (has_condition) {
		expression();
		consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

		exit_jump  = emit_jump(OP_JUMP_IF_FALSE);
		emit_byte(OP_POP);
	}

	if (!match(TOKEN_RIGHT_PAREN)) {
		uint32_t body_jump = emit_jump(OP_JUMP);
		uint32_t increment_start = current_chunk()->count;
		expression();
		emit_byte(OP_POP);
		// TODO: Don't require parentheses.
		consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
		emit_loop(loop_start);
		loop_start = increment_start;
		patch_jump(body_jump);
	}

	statement();

	emit_loop(loop_start);

	if (has_condition) {
		patch_jump(exit_jump);
		emit_byte(OP_POP);
	}

	end_scope();
}

static void return_statement() {
	if (current->type == FN_TYPE_SCRIPT) {
		error("Cannot return from top-level code.");
	}

	if (!check(TOKEN_SEMICOLON)) {
		expression();
	}
	consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
	emit_return();
}

static void and_(bool can_assign) {
	uint32_t end_jump = emit_jump(OP_JUMP_IF_FALSE);

	emit_byte(OP_POP);
	parse_precedence(PREC_AND);

	patch_jump(end_jump);
}

static void or_(bool can_assign) {
	uint32_t else_jump = emit_jump(OP_JUMP_IF_FALSE);
	uint32_t end_jump = emit_jump(OP_JUMP);

	patch_jump(else_jump);
	emit_byte(OP_POP);

	parse_precedence(PREC_OR);
	patch_jump(end_jump);
}

static void statement() {
	if (match(TOKEN_PRINT)) {
		print_statement();
	} else if (match(TOKEN_IF)) {
		// TODO: support if in expression position
		if_statement();
	} else if (match(TOKEN_RETURN)) {
		return_statement();
	} else if (match(TOKEN_FOR)) {
		for_statement();
	} else if (match(TOKEN_WHILE)) {
		while_statement();
	} else if (match(TOKEN_LEFT_BRACE)) {
		begin_scope();
		block();
		end_scope();
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

static bool identifiers_equal(Token *a, Token *b) {
	if (a->length != b->length) return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

static int32_t resolve_local(Compiler *compiler, Token *name) {
	for (int i = compiler->local_count - 1; i >= 0; i--) {
		Local *local = &compiler->locals[i];
		if (identifiers_equal(name, &local->name)) {
			if (local->depth == -1) {
				// Erroring is the default behavior. Instead, my version of Lox will
				// continue searching for similarly named variables in outer scopes.
				// error("Cannot read local variable in its own initializer.");

				continue;
			}
			return i;
		}
	}
	return -1;
}

static void named_variable(Token name, bool can_assign) {
	uint8_t get_op, set_op;

	uint32_t arg = resolve_local(current, &name);

	if (arg != -1) {
		if (arg > UINT8_MAX) {
			get_op = OP_GET_LOCAL_LONG;
			set_op = OP_SET_LOCAL_LONG;
		} else {
			get_op = OP_GET_LOCAL;
			set_op = OP_SET_LOCAL;
		}
	} else {
		arg = identifier_constant(&name);
		if (arg > UINT8_MAX) {
			get_op = OP_GET_GLOBAL_LONG;
			set_op = OP_SET_GLOBAL_LONG;
		} else {
			get_op = OP_GET_GLOBAL;
			set_op = OP_SET_GLOBAL;
		}
	}

	bool set = false;
	if (can_assign && match(TOKEN_EQUAL)) {
		expression();
		set = true;
	}
	emit_bytes(set ? set_op : get_op, arg & 0xff);
	if (arg > UINT8_MAX) {
		emit_bytes( (arg >> 8) & 0xff, (arg >> 16) & 0xff);
	}
}

static void variable(bool can_assign) {
	named_variable(parser.previous, can_assign);
}

static uint8_t argument_list() {
	uint8_t count = 0;
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			expression();
			if (count == UINT8_MAX) {
				error("Cannot have more than 255 arguments.");
			}
			count++;
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
	return count;
}

static void call(bool can_assign) {
	uint8_t arg_count = argument_list();
	emit_bytes(OP_CALL, arg_count);
}

ParseRule rules[] = {
	[TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_NONE},
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
	[TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_NONE},
	[TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_NONE},
	[TOKEN_GREATER]       = {NULL,     binary, PREC_NONE},
	[TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_NONE},
	[TOKEN_LESS]          = {NULL,     binary, PREC_NONE},
	[TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_NONE},
	[TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
	[TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
	[TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
	[TOKEN_AND]           = {NULL,     and_,   PREC_NONE},
	[TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
	[TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
	[TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
	[TOKEN_OR]            = {NULL,     or_,   PREC_NONE},
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


ObjectFunction* compile(const char *src) {
	scanner_init(src);

	Compiler compiler;
	compiler_init(&compiler, FN_TYPE_SCRIPT);

	parser.had_error = false;
	parser.panic_mode = false;

	advance();

	while (!match(TOKEN_EOF)) {
		declaration();
	}

	ObjectFunction *function = end_compilation();

	return parser.had_error ? NULL : function;
}
