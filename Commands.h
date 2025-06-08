#pragma once
#include <string>
#include <utility>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using std::pair;

enum class Command_type {
    report = 0,
    quit = 1,
    shutdown = 2,
    track = 3,
    untrack = 4,
};

struct Report_command {};
struct Quit_command {};
struct Shutdown_command {};
struct Track_command { std::string path; };
struct Untrack_command { std::string path; };

struct Command {
    Command_type type;

    union Data {
        Report_command   report;
        Quit_command     quit;
        Shutdown_command shutdown;
        Track_command    track;
        Untrack_command  untrack;

        Data() {}
        ~Data() {}
    } data;

    Command();
    Command(const Command& other);
    Command& operator=(const Command& other);
    ~Command();
};

pair<Command_type, bool> command_type_from_int(int id);
pair<Command, bool> command_from_json(const char* json_as_c_str);
pair<Command, bool> report(json* json);
pair<Command, bool> quit(json* json);
pair<Command, bool> shutdown(json* json);
pair<Command, bool> track(json* json);
pair<Command, bool> untrack(json* json);
