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

struct Time_session;

class Process_data {
private:

public:
    // NOTE(damian): might use some else for process identification. 
    // NOTE(damian): std::stringh just stores bytes. So any string can be stored, even UTF-8. 
    //               the behaviour of reading and decoding is then handed to the developer.
    string name;
    std::chrono::seconds time_spent;
    bool is_active;
    bool was_updated;
    std::chrono::steady_clock::time_point start;
    
    Process_data(wstring name, int time_spent = 0);
    Process_data(string name,  int time_spent = 0);
    ~Process_data();
    
    void update_active();
    void update_inactive();

};

// TEMP here
string wchar_to_utf8(const WCHAR* wchar_array);

// NOTE(damian): there are built in functions that we can overload from json packet, but it might want to do it like this for now.
int convert_from_json(const json* data, Process_data* process_data);
void convert_to_json(json* data, const Process_data* process_data);

