#pragma once
#include <chrono>
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <inttypes.h>
#include <Windows.h>

using nlohmann::json;
using std::string, std::wstring;
using std::vector;

class Process_data {
private:

public:
    // NOTE(damian): std::string just stores bytes. So any string can be stored, even UTF-8. 
    //               the behaviour of reading and decoding is then handed to the developer.
    
    class Session;

    string path;
    std::chrono::steady_clock::time_point start;
    vector<Process_data::Session> sessions;
    bool is_tracked;

    // These are to track the changes in states. 
    bool is_active;
    bool was_updated;

    // Process_data(wstring name, int time_spent = 0);
    Process_data(string path, int time_spent = 0);
    ~Process_data();
    
    void update_active();
    void update_inactive();

    bool operator==(const Process_data& other);

    class Session {
        using time_point = std::chrono::steady_clock::time_point; 
        // TODO(damian): these are nanoseconds, do some else.

        private: 

        public:
            time_point start_time;
            time_point end_time;

            Session(time_point start, time_point end);
            ~Session();
    };
    
};

// TEMP here
string wchar_to_utf8(const WCHAR* wchar_array);

// NOTE(damian): there are built in functions that we can overload from json packet, but it might want to do it like this for now.
int convert_from_json(const json* data, Process_data* process_data);
void convert_to_json(json* data, const Process_data* process_data);

