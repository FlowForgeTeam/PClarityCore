#pragma once
#include <chrono>
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <inttypes.h>
#include <Windows.h>

#include "Functions_win32.h"

using nlohmann::json;
using std::string, std::wstring;
using std::vector;

class Process_data {

public:
    class Session;

    Win32_process_data data;

    std::chrono::steady_clock::time_point start;
    vector<Process_data::Session> sessions;

    // These are to track the changes in states. 
    bool is_active;    // NOTE(damian): is a stored process currently active.
    bool is_tracked;   // NOTE(damian): this is used to determine, if a process has to have sessions created when declarated inactive.
    bool was_updated;  // NOTE(damian): used inside global state to simbolasi if the process was updates, if not --> it 
    
    // TODO(damian): maybe remove these flags and just store data and time here and session will be stored inside G_state inside a tuple:
    //               tracked_processes --> (Process_data, vec<Session>)
    //               regular_processes --> (Process_dasta)

    void update_active();
    void update_inactive();

    Process_data(Win32_process_data win32_data);

    bool operator==(const Process_data& other);
    //bool operator==(const Win32_process_data& win32_data);

    class Session {
        using time_point = std::chrono::steady_clock::time_point; 
        // TODO(damian): these are nanoseconds, do some else.

        public:
            time_point start_time;
            time_point end_time;

            Session(time_point start, time_point end);
            ~Session();
    };
    
};

// NOTE(damian): there are built in functions that we can overload from json packet, but it might want to do it like this for now.
int convert_from_json(const json* data, Process_data* process_data);
void convert_to_json(json* data, const Process_data* process_data);

