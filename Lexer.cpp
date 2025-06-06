#pragma once
#include <iostream>
#include "Lexer.h"

Lexer lexer_init(const char* text) {
    Lexer lexer           = { 0 };
    lexer.text            = text;
    lexer.text_len        = strlen(text);
    lexer.token_start_idx = 0;
    lexer.current_idx     = 0;

    return lexer;
}

Lexer lexer_init(const char* text, size_t text_len) {
    Lexer lexer           = {0};
    lexer.text            = text;
    lexer.text_len        = text_len;
    lexer.token_start_idx = 0;
    lexer.current_idx     = 0;

    return lexer;
}

bool lexer_is_at_end(Lexer* lexer) {
    if (lexer->current_idx < lexer->text_len)
        return false;
    else if (lexer->current_idx == lexer->text_len)
        return true;
    else {
        // printf("BACK_END_ERROR: Somehow lexer index went out of the text char array. \n");
        // exit(1);
        // TODO(damian): handle better.
    }
}

char lexer_peek_next_char(Lexer *lexer) {
    if (lexer_is_at_end(lexer))
        return '\0';
    else
        return lexer->text[lexer->current_idx];
}

char lexer_peek_nth_char(Lexer* lexer, u32 n) {
    if (n < 0) {
        // printf("BACK_END_ERROR: 'lexer_peek_nth_char' can't have n < 0, but n: %d was passed in. \n", n);
        // exit(1);
        // TODO(damian): handle better.
    }
    if (lexer->current_idx + n < lexer->text_len) 
        return lexer->text[lexer->current_idx + n];
    else 
        return '\0';
}

char lexer_consume_char(Lexer* lexer) {
    if (lexer_is_at_end(lexer))
        return '\0';
    else {
        char consumed_char  = lexer->text[lexer->current_idx];
        lexer->current_idx += 1;

        return consumed_char;
    }
}

Token lexer_next_token(Lexer* lexer) {
    while (lexer_skip_whitespaces(lexer) == true); 

    // Setting up data for the upcomming token
    char prev_char = lexer_consume_char(lexer);

    switch (prev_char) {
        case '\0': {
            return lexer_init_token(lexer, Token_type::eof);
        } break;

        case 'p': {
            return lexer_match_full_keyword(lexer, 1, "clarity_cmd", 11, Token_type::pclarity_cmd);
        } break;

        case 'r': {
            return lexer_match_full_keyword(lexer, 1, "eport", 5, Token_type::report);
        } break;

        case 'q': {
            return lexer_match_full_keyword(lexer, 1, "uit", 3, Token_type::quit);
        } break;

        case 's': {
            return lexer_match_full_keyword(lexer, 1, "hutdown", 7, Token_type::shutdown);
        } break;

        case 't': {
            return lexer_match_full_keyword(lexer, 1, "rack", 4, Token_type::track);
        } break;

        case 'u': {
            return lexer_match_full_keyword(lexer, 1, "ntrack", 6, Token_type::untrack);
        } break;

        default: {
            if (prev_char == '@')
                return lexer_create_string_token(lexer);

            return lexer_init_token(lexer, Token_type::unsupported);
        } break;
    }
}

Token lexer_peek_next_token(Lexer* lexer) {
    // Only storing end values, since starts are changed to ends when a new token is created
    u32 token_start_idx = lexer->current_idx;
    Token token         = lexer_next_token(lexer);
    lexer->current_idx  = token_start_idx;

    return token;
}

Token lexer_peek_nth_token(Lexer* lexer, u32 n) {
    if (n <= 0) {
        // printf("BACK_END_ERROR: 'lexer_peek_nth_token' cant have n <= 0, but n: %d was passed in. \n", n);
        // exit(1);
        // TODO(damian): handle better.
    }

    // Only storing end values, since start are changed to end when token is created
    u32 token_start_idx = lexer->current_idx;

    // TODO: implement this
    //       Alredy created token via peeking should be stored into a Token buffer.
    Token token;
    for (int i=0; i<n; ++i)
        token = lexer_next_token(lexer);

    lexer->current_idx     = token_start_idx;

    return token;
}

bool lexer_match_token(Lexer* lexer, Token_type expected_type) {
    Token next_token = lexer_peek_next_token(lexer);
    if (next_token.type == expected_type) {
        lexer_next_token(lexer);
        return true;
    }
    else 
        return false;
}

Token lexer_create_string_token(Lexer* lexer) {
    while (lexer_peek_next_char(lexer) != '@' && !lexer_is_at_end(lexer)) {
        lexer_consume_char(lexer);
    }

    if (lexer->text[lexer->current_idx] == '@') {
        lexer_consume_char(lexer);
        return lexer_init_token(lexer, Token_type::path);
    }
    else {
        /*printf("Was not able to parse a string, closing '\"' wasn't found. \n");
        exit(1);*/
        // TODO(damian): handle this better.
    }
}

// TODO(damian): remove all these nested ifs.
Token lexer_match_full_keyword(Lexer* lexer, u32 current_token_offset, const char* rest, u32 rest_len, Token_type type_to_match) {
    
    for (int i=0; i<rest_len; ++i) {
        if (lexer_peek_next_char(lexer) != rest[i]) {
            return lexer_init_token(lexer, Token_type::unsupported);
        }
        lexer_consume_char(lexer);
    }
    
    return lexer_init_token(lexer, type_to_match);
}

bool lexer_skip_whitespaces(Lexer* lexer) {
    bool skipped = false;
    while ( isspace(lexer_peek_next_char(lexer)) ) {
        skipped = true;
        lexer_consume_char(lexer);
    }   
    
    lexer->token_start_idx = lexer->current_idx;
    return skipped;
}

Token lexer_init_token(Lexer* lexer, Token_type type) {
    Token token  = {};
    token.type   = type;
    token.lexeme = lexer->text + lexer->token_start_idx;
    token.length = lexer ->current_idx - lexer->token_start_idx;

    return token;
}

void token_print(Token* token) {
    using std::cout, std::endl;

    cout << "Token: " << endl;
    cout << "    type  : " << (int) token->type << endl;
    
    cout << "    lexeme: " << "\"";   
    for (int i = 0; i < token->length; i++) {
        cout << token->lexeme[i];
    }
    cout << "\"" << endl;   
}

