#include <assert.h>
#include "Parser.h"

// Grammar:
//		"pclarity_cmd" <command> (extra_args)?

Parser parser_init(const char* text) {
	Parser parser = {};
	parser.lexer  = lexer_init(text);
	return parser;
}



std::pair<Command, bool> start_parsing(Parser* parser) {
    bool matched = lexer_match_token(&parser->lexer, Token_type::pclarity_cmd);

    if (matched) {
        Command parser_command = command(&parser->lexer);

        if (lexer_match_token(&parser->lexer, Token_type::eof)) 
            return pair(parser_command, true);
        else {
            // TODO(damian): handle better.
            return pair(Command(), false);
        }
    }
    else {
        // TODO(damian): handle invalid command.
        return pair(Command(), false);
    }

}

Command command(Lexer* lexer) {
    Token token = lexer_next_token(lexer);

    switch (token.type) {
        case Token_type::report: {
            return report(lexer);
        } break;

        case Token_type::quit: {
            return quit(lexer);
        } break;

        case Token_type::shutdown: {
            return shutdown(lexer);
        } break;

        case Token_type::track: {
            return track(lexer);
        } break;

        case Token_type::untrack: {
            return untrack(lexer);
        } break;

        default: {
            // TODO(damian): handle better.
            assert(false);
        } break;

    }

}

Command report(Lexer* lexer) {
    Command new_command = {};
    new_command.type    = Command_type::report;
    new_command.report  = Report_command();
    
    return new_command;
}

Command quit(Lexer* lexer) {
    Command new_command = {};
    new_command.type    = Command_type::quit;
    new_command.quit    = Quit_command();
     
    return new_command;
}

Command shutdown(Lexer* lexer) {
    Command new_command  = {};
    new_command.type     = Command_type::shutdown;
    new_command.shutdown = Shutdown_command();
    
    return new_command;
}

Command track(Lexer* lexer) {
    Token token = lexer_next_token(lexer);

    if (token.type != Token_type::path) {
        // TODO(damian): handle better.
        assert(false);
    }

    Track_command track;
    track.path_token = token;

    Command new_command = {};
    new_command.type    = Command_type::track;
    new_command.track   = track;
    
    return new_command;
}

Command untrack(Lexer* lexer) {
    Token token = lexer_next_token(lexer);

    if (token.type != Token_type::path) {
        // TODO(damian): handle better.
        assert(false);
    }

    Untrack_command untrack;
    untrack.path_token = token;

    Command new_command = {};
    new_command.type    = Command_type::untrack;
    new_command.untrack = untrack;
    
    return new_command;
}


















