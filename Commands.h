#pragma once
#include <string>
#include <utility>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using std::pair;

enum class Command_type {
    report   = 0,
    quit     = 1,
    shutdown = 2,
    track    = 3,
    untrack  = 4,
    grouped_report = 5,
};

struct Report_command {};
struct Quit_command {};
struct Shutdown_command {};
struct Track_command { std::string path; };
struct Untrack_command { std::string path; };
struct Grouped_report_command {};

struct Command {
    Command_type type;

    union Data {
        Report_command   report;
        Quit_command     quit;
        Shutdown_command shutdown;
        Track_command    track;
        Untrack_command  untrack;
        Grouped_report_command grouped_report;

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
pair<Command, bool> report        (json* j);
pair<Command, bool> quit          (json* j);
pair<Command, bool> shutdown      (json* j);
pair<Command, bool> track         (json* j);
pair<Command, bool> untrack       (json* j);
pair<Command, bool> grouped_report(json* j);












