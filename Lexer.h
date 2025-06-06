#pragma once
#include <inttypes.h>

typedef uint32_t u32;

enum class Token_type {
    pclarity_cmd,

    report,
    quit,
    shutdown,
    track,
    untrack,

    path,

    eof,
    unsupported,
};

struct Token {
    Token_type  type;

    const char* lexeme;
    size_t      length;
};

struct Lexer {
    const char* text;
    size_t  text_len;

    u32 token_start_idx;
    u32 current_idx;
};

Lexer lexer_init(const char* text);
Lexer lexer_init(const char* text, size_t text_len);

bool lexer_is_at_end     (Lexer* lexer);
char lexer_peek_next_char(Lexer* lexer);
char lexer_peek_nth_char (Lexer* lexer, u32 n);
char lexer_consume_char  (Lexer* lexer);

Token lexer_next_token     (Lexer* lexer);
Token lexer_peek_next_token(Lexer* lexer);
Token lexer_peek_nth_token (Lexer* lexer, u32 n);
bool  lexer_match_token    (Lexer* lexer, Token_type expected_type);

Token lexer_create_string_token(Lexer *lexer);
Token lexer_match_full_keyword (Lexer* lexer, u32 current_token_offset, const char* rest, u32 rest_len, Token_type type_to_match);
bool  lexer_skip_whitespaces   (Lexer* lexer);
Token lexer_init_token         (Lexer* lexer, Token_type type);

void  token_print(Token* token);