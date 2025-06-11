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
    // NOTE(damian): std::string just stores bytes. So any string can be stored, even UTF-8. 
    //               the behaviour of reading and decoding is then handed to the developer.
    
    class Session;

    Win32_process_data data;

    std::chrono::steady_clock::time_point start;
    vector<Process_data::Session> sessions;

    // These are to track the changes in states. 
    bool is_active;
    bool is_tracked;
    bool was_updated;

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

