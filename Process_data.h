#pragma once
#include <chrono>
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <inttypes.h>
#include <Windows.h>

#include "Functions_win32.h"

using nlohmann::json;
using std::vector;
using std::chrono::steady_clock; // NOTE(damian): steady clock is used for interval measures (it is more precise).
using std::chrono::system_clock; // NOTE(damian): system clock is used for time/date storage, so i use it here to know when the date when the session started.

class Process_data {

public:
    class Session;

    // NOTE(damian): these are used later for process creation when the process ends.
    steady_clock::time_point steady_start; 
    system_clock::time_point system_start; 
    
    vector<Session> sessions;
    Win32_process_data data;

    // These are to track the changes in states. 
    bool is_active;    // NOTE(damian): is a stored process currently active.
    bool is_tracked;   // NOTE(damian): this is used to determine, if a process has to have sessions created when declarated inactive.
    bool was_updated;  // NOTE(damian): used inside global state to simbolasi if the process was updates, if not --> it 

    // TODO(damian): maybe remove these flags and just store data and time here and session will be stored inside G_state inside a tuple:
    //               tracked_processes --> (Process_data, vec<Session>)
    //               regular_processes --> (Process_dasta)

    void update_active();
    void update_inactive();
    void update_data(Win32_process_data* new_win32_data);

    Process_data(Win32_process_data win32_data);

    bool operator==(const Process_data& other);

    struct Session {
            steady_clock::time_point steady_start_time;
            steady_clock::time_point steady_end_time;

            system_clock::time_point system_start_time;
            system_clock::time_point system_end_time;

            Session(system_clock::time_point system_start, system_clock::time_point system_end,
                    steady_clock::time_point steady_start, steady_clock::time_point steady_end);
    };
    
};



// NOTE(damian): there are built in functions that we can overload from json packet, but it might want to do it like this for now.
bool convert_from_json(Process_data* process_data, json* j);
void convert_to_json  (Process_data* process_data, json* j);

