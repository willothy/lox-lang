#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scanner.h"
#include "chunk.h"
#include "object.h"

typedef struct {
	char* start;
	char* current;
	Linenr line;
	size_t offset;
} Scanner;

Scanner scanner;

static Token token(TokenType type) {
	Token token;
	token.type = type;
	token.start = scanner.start;
	token.length = (size_t)(scanner.current - scanner.start);
	token.line = scanner.line;
	return token;
}

static bool is_at_end() {
	return *scanner.current == '\0';
}

static bool is_digit(char c){
	return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
	return (c >= 'a' && c <= 'z') ||
	       (c >= 'A' && c <= 'Z') ||
	       c == '_';
}

static char advance() {
	scanner.offset++;
	return *scanner.current++;
}

static char peek() {
	return *scanner.current;
}

static char peek_next() {
	if (is_at_end()) return '\0';
	return scanner.current[1];
}

static bool match(char expected) {
	if (is_at_end()) return false;
	if (peek() != expected) return false;
	scanner.current++;
	return true;
}

static bool skip_whitespace() {
	bool newline = false;
	for (;;) {
		char c = peek();
		switch (c) {
		case ' ':
		case '\r':
		case '\t':
			advance();
			break;
		case '\n': {
			scanner.line++;
			advance();
			newline = true;
			break;
		}
		case '/': {
			if (peek_next() == '/') {
				while (peek() != '\n' && !is_at_end()) advance();
			} else {
				return newline;
			}
		}
		default:
			return newline;
		}
	}
	return newline;
}

static Token error_token(const char* format, ...) {
	va_list args;
	char *message = malloc(1024);
	va_start(args, format);
	size_t len = vsnprintf(message, 1024, format, args);
	va_end(args);

	ObjectString *msg = copy_string(message, len);

	Token token;
	token.type = TOKEN_ERROR;
	token.start = msg->chars;
	token.length = msg->length;
	token.line = scanner.line;
	return token;
}

static Token string() {
	while (peek() != '"' && !is_at_end()) {
		if (peek() == '\n') scanner.line++;
		advance();
	}

	if (is_at_end()) {
		return error_token("Unterminated string.");
	}

	advance();
	return token(TOKEN_STRING);
}

static Token number() {
	while (is_digit(peek())) advance();

	if (peek() == '.' && is_digit(peek_next())) {
		advance();
		while (is_digit(peek())) advance();
	}
	return token(TOKEN_NUMBER);
}

static TokenType check_keyword(size_t start, size_t length, const char* rest, TokenType type) {
	if (scanner.current - scanner.start == start + length &&
	    memcmp(scanner.start + start, rest, length) == 0) {
		return type;
	}
	return TOKEN_IDENTIFIER;
}

static TokenType ident_type() {
	switch (*scanner.start) {
	// TODO: use tries
	case 'a': return check_keyword(1, 2, "nd", TOKEN_AND);
	case 'c': return check_keyword(1, 4, "lass", TOKEN_CLASS);
	case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
	case 'f': {
		if (scanner.current - scanner.start > 1) {
			switch(scanner.start[1]) {
			case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
			case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
			case 'u': return check_keyword(2, 1, "n", TOKEN_FUN);
			}
		}
		break;
	}
	case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
	case 'n': return check_keyword(1, 2, "il", TOKEN_NIL);
	case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
	// case 'p': return check_keyword(1, 4, "rint", TOKEN_PRINT);
	case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
	case 's': return check_keyword(1, 4, "uper", TOKEN_SUPER);
	case 't': {
		if (scanner.current - scanner.start > 1) {
			switch (scanner.start[1]) {
			case 'h': return check_keyword(2, 2, "is", TOKEN_THIS);
			case 'r': return check_keyword(2, 2, "ue", TOKEN_TRUE);
			}
		}
		break;
	}
	case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
	case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
	}

	return TOKEN_IDENTIFIER;
}

static Token ident() {
	while (is_alpha(peek()) || is_digit(peek())) advance();
	return token(ident_type());
}

void scanner_init(char* source) {
	scanner.start = source;
	scanner.current = source;
	scanner.line = 1;
	scanner.offset = 0;
}

Token scanner_next_token() {
	bool newline = skip_whitespace();

	scanner.start = scanner.current;
	if (newline) {
		return token(TOKEN_NEWLINE);
	}

	if (is_at_end()) {
		return token(TOKEN_EOF);
	}

	char c = advance();

	switch (c) {
	// single-character tokens
	case '(': return token(TOKEN_LEFT_PAREN);
	case ')': return token(TOKEN_RIGHT_PAREN);
	case '{': return token(TOKEN_LEFT_BRACE);
	case '}': return token(TOKEN_RIGHT_BRACE);
	case '[': return token(TOKEN_LEFT_BRACKET);
	case ']': return token(TOKEN_RIGHT_BRACKET);
	case ';': return token(TOKEN_SEMICOLON);
	case ',': return token(TOKEN_COMMA);
	case ':': return token(TOKEN_COLON);
	case '.':
		if (match('.')) {
			return token(TOKEN_DOUBLE_DOT);
		}
		return token(TOKEN_DOT);
	case '-': {
		if (match('>')) {
			return token(TOKEN_ARROW);
		} else if (match('=')) {
			return token(TOKEN_MINUS_EQUAL);
		}

		return token(TOKEN_MINUS);
	}
	case '+': {
		if (match('=')) {
			return token(TOKEN_PLUS_EQUAL);
		}
		return token(TOKEN_PLUS);
	}
	case '/': {
		if (match('=')) {
			return token(TOKEN_SLASH_EQUAL);
		}
		return token(TOKEN_SLASH);
	}
	case '*': {
		if (match('=')) {
			return token(TOKEN_STAR_EQUAL);
		}
		return token(TOKEN_STAR);
	}

	// complex tokens
	case '!': {
		return token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
	}
	case '=': {
		return token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
	}
	case '<': {
		return token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
	}
	case '>': {
		return token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
	}

	// string
	case '"': {
		return string();
	}
	}

	if (is_alpha(c)) {
		return ident();
	}
	if (is_digit(c)) {
		return number();
	}

	return error_token("Unexpected character %c (%d).", c, c);
}
