#ifndef clox_scanner_h
#define clox_scanner_h

#include "chunk.h"

typedef enum {
  // Single-character tokens.
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_BRACKET,
  TOKEN_RIGHT_BRACKET,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_SEMICOLON,

  // One or two character tokens.
  TOKEN_DOT,
  TOKEN_DOUBLE_DOT,
  TOKEN_MINUS,
  TOKEN_MINUS_EQUAL,
  TOKEN_ARROW,
  TOKEN_PLUS,
  TOKEN_PLUS_EQUAL,
  TOKEN_SLASH,
  TOKEN_SLASH_EQUAL,
  TOKEN_STAR,
  TOKEN_STAR_EQUAL,

  TOKEN_BANG,
  TOKEN_BANG_EQUAL,
  TOKEN_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER,
  TOKEN_GREATER_EQUAL,
  TOKEN_LESS,
  TOKEN_LESS_EQUAL,

  // Literals.
  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_NUMBER,

  // Keywords.
  TOKEN_AND,
  TOKEN_CLASS,
  TOKEN_ELSE,
  TOKEN_FALSE,
  TOKEN_FOR,
  TOKEN_FUN,
  TOKEN_IF,
  TOKEN_IN,
  TOKEN_NIL,
  TOKEN_OR,
  TOKEN_RETURN,
  TOKEN_SUPER,
  TOKEN_THIS,
  TOKEN_TRUE,
  TOKEN_VAR,
  TOKEN_WHILE,
  TOKEN_COROUTINE,
  TOKEN_CONTINUE,
  TOKEN_BREAK,
  TOKEN_YIELD,
  TOKEN_AWAIT,

  TOKEN_ERROR,
  TOKEN_EOF,
  TOKEN_NEWLINE
} TokenType;

typedef struct {
  TokenType type;
  char *start;
  size_t length;
  Linenr line;
} Token;

void scanner_init(char *source);
Token scanner_next_token();

#endif
