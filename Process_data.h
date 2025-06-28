#pragma once
#include <chrono>
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <inttypes.h>
#include <Windows.h>
#include <optional>

#include "Functions_win32.h"

using nlohmann::json;
using std::vector;
using std::optional;
using std::pair;

// Predeclare for G_state
namespace G_state {
    enum class Error_type;
    struct Error;
}
using G_state::Error, G_state::Error_type;

struct Session;

class Process_data {

public:
    // These are to track the changes in states. 
    bool is_active;    // NOTE(damian): is a stored process currently active.
    bool is_tracked;   // NOTE(damian): this is used to determine, if a process has to have sessions created when declarated inactive.
    bool was_updated;  // NOTE(damian): used inside global state to simbolase if the process was updates, if not, a deleting might be performed. 
    
    // TODO(damian): do i really need this, i guess not really, but this might be better for the client, to be able to not convert path all the time for processes with not icons. MAYBE.
    bool has_image;
    
    // These are used later for process creation when the process ends.
    optional<std::chrono::steady_clock::time_point> steady_start;
    optional<std::chrono::system_clock::time_point> system_start;

    optional<std::chrono::steady_clock::time_point> last_time_session_was_created;

    // Regular data
    optional<Win32_snapshot_data>  snapshot;
    optional<string>               product_name;
    optional<DWORD>                priority_class;
    optional<Win32_affinities>     affinities;
    optional<SIZE_T>               ram_usage;

    // Special data, might have different update patterns and stuff
    string                        exe_path;
    optional<Win32_process_times> times;
    optional<float>               cpu_usage;

    Process_data(string exe_path);
    Process_data(Win32_process_data* win32_data);

    pair<Error, optional<Session>> update_active();
    pair<Error, optional<Session>> update_inactive();
    Error update_data(Win32_process_data* new_win32_data);
    void reset_data();

    pair<Error, bool> compare(Process_data*        other);
    pair<Error, bool> compare (Win32_process_data* data);

    bool compare_as_tracked(Process_data*       other);
    bool compare_as_tracked(Win32_process_data* data);
};

using std::chrono::steady_clock; // NOTE(damian): steady clock is used for interval measures (it is more precise).
using std::chrono::system_clock; // NOTE(damian): system clock is used for time/date storage, so i use it here to know when the date when the session started.
using std::chrono::duration;
using std::chrono::seconds;

struct Session {
        seconds duration_sec;
        seconds system_start_time_in_seconds;
        seconds system_end_time_in_seconds;   

        Session() = default;
        Session(seconds duration_sec, 
                seconds system_start, 
                seconds system_end);
};

// NOTE(damian): there are built in functions that we can overload from json packet, but it might want to do it like this for now.
//bool convert_from_json(Process_data* process_data, json* j);
void convert_to_json  (Process_data* process_data, json* j);





















