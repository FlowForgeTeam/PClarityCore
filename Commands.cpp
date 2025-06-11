#include "Commands.h"
#include <new>
#include <utility>
using namespace std;

Command::Command() {
    type = Command_type::report;
    new(&data.report) Report_command();
}

Command::Command(const Command& other) {
    type = other.type;
    switch (type) {
        case Command_type::report:   new(&data.report)   Report_command(other.data.report);     break;
        case Command_type::quit:     new(&data.quit)     Quit_command(other.data.quit);         break;
        case Command_type::shutdown: new(&data.shutdown) Shutdown_command(other.data.shutdown); break;
        case Command_type::track:    new(&data.track)    Track_command(other.data.track);       break;
        case Command_type::untrack:  new(&data.untrack)  Untrack_command(other.data.untrack);   break;
    }
}

Command& Command::operator=(const Command& other) {
    if (this == &other) return *this;

    // Destroy current
    this->~Command();

    // Copy
    type = other.type;
    switch (type) {
        case Command_type::report:   new(&data.report)   Report_command(other.data.report);     break;
        case Command_type::quit:     new(&data.quit)     Quit_command(other.data.quit);         break;
        case Command_type::shutdown: new(&data.shutdown) Shutdown_command(other.data.shutdown); break;
        case Command_type::track:    new(&data.track)    Track_command(other.data.track);       break;
        case Command_type::untrack:  new(&data.untrack)  Untrack_command(other.data.untrack);   break;
    }

    return *this;
}

Command::~Command() {
    switch (type) {
        case Command_type::report:   data.report.~Report_command();     break;
        case Command_type::quit:     data.quit.~Quit_command();         break;
        case Command_type::shutdown: data.shutdown.~Shutdown_command(); break;
        case Command_type::track:    data.track.~Track_command();       break;
        case Command_type::untrack:  data.untrack.~Untrack_command();   break;
    }
}

// ============================================

pair<Command_type, bool> command_type_from_int(int id) {
    switch (id) {
        case 0:  return pair(Command_type::report,   true);
        case 1:  return pair(Command_type::quit,     true);
        case 2:  return pair(Command_type::shutdown, true);
        case 3:  return pair(Command_type::track,    true);
        case 4:  return pair(Command_type::untrack,  true);
        default: return pair(Command_type::report,   false);
    }
}

pair<Command, bool> command_from_json(const char* json_as_c_str) {
    json j = json::parse(json_as_c_str, nullptr, false);
    if (j.is_discarded() || !j.contains("command_id") || !j["command_id"].is_number()) {
        return pair(Command(), false);
    }

    auto result = command_type_from_int(j["command_id"]);
    if (!result.second) return pair(Command(), false);

    switch (result.first) {
            case Command_type::report:   return report(&j);
            case Command_type::quit:     return quit(&j);
            case Command_type::shutdown: return shutdown(&j);
            case Command_type::track:    return track(&j);
            case Command_type::untrack:  return untrack(&j);
            default:                     return pair(Command(), false);
    }
}

pair<Command, bool> report(json*) {
    Command cmd;
    cmd.type = Command_type::report;
    new(&cmd.data.report) Report_command();
    return pair(cmd, true);
}

pair<Command, bool> quit(json*) {
    Command cmd;
    cmd.type = Command_type::quit;
    new(&cmd.data.quit) Quit_command();
    return pair(cmd, true);
}

pair<Command, bool> shutdown(json*) {
    Command cmd;
    cmd.type = Command_type::shutdown;
    new(&cmd.data.shutdown) Shutdown_command();
    return pair(cmd, true);
}

pair<Command, bool> track(json* j) {
    if (!j->contains("extra") || !(*j)["extra"].contains("path") || !(*j)["extra"]["path"].is_string()) {
        return pair(Command(), false);
    }

    Command cmd;
    cmd.type = Command_type::track;
    new(&cmd.data.track) Track_command{ (*j)["extra"]["path"].get<string>() };
    return pair(cmd, true);
}

pair<Command, bool> untrack(json* j) {
    if (!j->contains("extra") || !(*j)["extra"].contains("path") || !(*j)["extra"]["path"].is_string()) {
        return pair(Command(), false);
    }

    Command cmd;
    cmd.type = Command_type::untrack;
    new(&cmd.data.untrack) Untrack_command{ (*j)["extra"]["path"].get<string>() };
    return pair(cmd, true);
}
