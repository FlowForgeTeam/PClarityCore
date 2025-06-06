#pragma once
#include <utility>
#include "Lexer.h"

using std::pair;

enum class Command_type {
	report,
    quit,
    shutdown,
    track,
    untrack
};

struct Report_command {

};

struct Quit_command {

};

struct Shutdown_command {

};

struct Track_command {
    Token path_token;

};

struct Untrack_command {
    Token path_token;

};


struct Command {
	Command_type type;
	union {
        Report_command   report;
        Quit_command     quit;
        Shutdown_command shutdown;
        Track_command    track;
        Untrack_command  untrack;
	};
};

struct Parser {
	Lexer lexer;
};

Parser parser_init(const char* text);

std::pair<Command, bool> start_parsing(Parser* lexer);

Command command (Lexer* lexer);
Command report  (Lexer* lexer);
Command quit    (Lexer* lexer);
Command shutdown(Lexer* lexer);
Command track   (Lexer* lexer);
Command untrack (Lexer* lexer);





