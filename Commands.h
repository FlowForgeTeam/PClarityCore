#pragma once
#include <optional>

enum class Command {
    report = 0,
    quit,
    shutdown,
    track,
    untrack,

};

std::optional<Command> command_from_int(int id);



