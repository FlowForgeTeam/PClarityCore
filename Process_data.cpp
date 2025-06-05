#include "Process_data.h"

// NOTE(damian): this was claude generated, need to make sure its is correct. 
std::string wchar_to_utf8(const WCHAR* wstr) {
    if (!wstr) return "";

    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";

    std::string result(size - 1, 0); // size-1 to exclude null terminator
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], size, nullptr, nullptr);

    return result;
}

Process_data::Process_data(string path, int time_spent) {
    this->path        = path;
    this->start       = std::chrono::steady_clock::now();
    this->sessions    = std::vector<Process_data::Session>();

    this->is_active   = false;
    this->was_updated = false;
}

Process_data::~Process_data() {}

// Updating a process as if its new state is active
void Process_data::update_active() {
    if (this->is_active == false) {
        this->is_active = true;
        this->start     = std::chrono::steady_clock::now();
    } 
    else {
        // Nothing here
    }

    this->was_updated = true;
}

// Updating a process as if its new state is in_active
void Process_data::update_inactive() {
    if (this->is_active == true) {
        this->is_active   = false;

        Process_data::Session session(this->start, std::chrono::steady_clock::now());
        this->sessions.push_back(session);
    }
    else {
        // Nothing here
    }

    this->was_updated = true;
}


bool Process_data::operator==(const Process_data& other) {
    return this->path == other.path;
}

// == Process_data::Session =================================================================

Process_data::Session::Session(std::chrono::steady_clock::time_point start,  std::chrono::steady_clock::time_point end) {
    this->start_time = start;
    this->end_time   = end;
}

Process_data::Session::~Session() {}

// == Just some functions ===================================================================

int convert_from_json(const json* data, Process_data* process_data) {
    if (   data->contains("process_name")
        && data->contains("process_path")
        && data->contains("sessions")
    ) {
        process_data->path       = (*data)["process_path"];
        process_data->is_active  = false;

        // TODO(damian): i fate this, this is error prone. There should be something like an optional type that we use for values like start_time, before the process was even initialised for tracking.
        process_data->start      = std::chrono::steady_clock::now(); 
                    
        vector<json> sessions = (*data)["sessions"];
        for (json& j_session : sessions) {
            if (   j_session.contains("start_time")
                && j_session.contains("end_time")
            ) {
                using time_point = std::chrono::steady_clock::time_point; 
                using duration   = std::chrono::steady_clock::duration; 

                long long start_as_int64 = j_session["start_time"];
                long long end_as_int64   = j_session["end_time"];
                
                duration start_as_duration = duration(start_as_int64);
                duration end_as_duration  = duration(end_as_int64);     

                time_point start_as_time_point = time_point(start_as_duration);
                time_point end_as_time_point   = time_point(end_as_duration); 

                Process_data::Session session(start_as_time_point, end_as_time_point);
                process_data->sessions.push_back(session);
            }

        }

        return 0;
    }
    else {
        return 1;
    }
}

#include <iostream>

void convert_to_json(json* data, const Process_data* process_data) {
    size_t idx = process_data->path.find_last_of("\\");

    (*data)["process_name"] = process_data->path.c_str() + idx + 1;
    (*data)["process_path"] = process_data->path;
    
    (*data)["last_time_was_active"] = process_data->is_active;

    vector<json> sessions_as_jsons;
    for (auto& session : process_data->sessions) {
        json j_session;
        j_session["start_time"] = session.start_time.time_since_epoch().count();
        j_session["end_time"]   = session.end_time.time_since_epoch().count();

        sessions_as_jsons.push_back(j_session);
    }

    (*data)["sessions"] = sessions_as_jsons;

}


