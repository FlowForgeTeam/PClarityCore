#include "Commands.h"

std::optional<Command> command_from_int(int id) {
    switch (id) {
        case 0: {
            return Command::report;
        } break;

        case 1: {
            return Command::quit;
        } break;

        case 2: {
            return Command::shutdown;
        } break;

        case 3: {
            return Command::track;
        } break;

        case 4: {
            return Command::untrack;
        } break;

        default: {
            return std::nullopt;
        } break;

    }

}