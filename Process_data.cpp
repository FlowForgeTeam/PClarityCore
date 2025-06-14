#include "Process_data.h"

Process_data::Process_data(Win32_process_data win32_data) {
    this->data        = win32_data;
    this->sessions    = std::vector<Process_data::Session>();

    // TODO(damian): maybe have this as void start_session().
    this->steady_start = std::chrono::steady_clock::now();
    this->system_start = std::chrono::system_clock::now();

    this->is_tracked  = false;
    this->is_active   = false;
    this->was_updated = false;
}

// Updating a process as if its new state is active
void Process_data::update_active() {
    if (this->is_active == false) {
        this->is_active    = true;
        this->steady_start = std::chrono::steady_clock::now();
        this->system_start = std::chrono::system_clock::now();
    } 
    else {
        // Nothing here
    }

    
    this->was_updated = true;
}

// Updating a process as if its new state is inactive
void Process_data::update_inactive() {
    if (this->is_active == true) {
        this->is_active = false;
        
        if (this->is_tracked) {
            Process_data::Session session(this->system_start, system_clock::now(), 
                                          this->steady_start,steady_clock::now());

            this->sessions.push_back(session);
        }
    }
    else {
        // Nothing here
    }

    this->was_updated = true;
}

void Process_data::update_data(Win32_process_data* new_win32_data) {
    this->data = *new_win32_data;
}

//bool Process_data::operator==(const Win32_process_data& win32_data) {
//    if (this->is_tracked) {
//        return this->data.exe_path == win32_data.exe_path;
//    }
//    else {
//        return (
//               this->data.exe_path == win32_data.exe_path
//            && this->data.creation_time == win32_data.creation_time
//        );
//    }
//}

bool Process_data::operator==(const Process_data& other) {
    if (other.data.exe_name == "Telegram.exe") {
        int x = 2;
    }

    if (this->is_tracked) {
        return this->data.exe_path == other.data.exe_path;
    }
    else {
        return (
               this->data.exe_path      == other.data.exe_path 
            && this->data.creation_time == other.data.creation_time
        );
    }
}

//bool Process_data::operator==(const Win32_process_data& win32_data) {
//    return (
//           this->data.exe_path      == win32_data.exe_path 
//        && this->data.creation_time == win32_data.creation_time
//    );
//}


// == Process_data::Session =================================================================

Process_data::Session::Session(system_clock::time_point system_start, system_clock::time_point system_end,
                               steady_clock::time_point steady_start, steady_clock::time_point steady_end
) {
    this->system_start_time = system_start;
    this->system_end_time   = system_end;

    this->steady_start_time = steady_start;
    this->steady_end_time   = steady_end;
}

// == Just some functions ===================================================================

bool convert_from_json(Process_data* process_data, json* j) {
    using std::chrono::time_point_cast;
    using std::chrono::seconds;
    using std::chrono::nanoseconds;
    using std::chrono::steady_clock;
    using std::chrono::system_clock;

    try {
        auto steady_start_seconds  = seconds((*j)["steady_start"].get<int64_t>());
        process_data->steady_start = steady_clock::time_point(steady_start_seconds);

        auto system_start_seconds  = seconds((*j)["system_start"].get<int64_t>());
        process_data->system_start = system_clock::time_point(system_start_seconds);
    
        for (json& j_session : (*j)["sessions"]) {
            auto system_start_seconds = seconds(j_session["system_start_time"].get<int64_t>());
            auto system_end_seconds   = seconds(j_session["system_end_time"].get<int64_t>());
            
            auto steady_start_seconds = seconds(j_session["steady_start_time"].get<int64_t>());
            auto steady_end_seconds   = seconds(j_session["steady_end_time"].get<int64_t>());

            // NOTE(damian): cant push_back(session) for some C++ reson.
            /*Process_data::Session session(system_clock::time_point(system_start_seconds),
                                          system_clock::time_point(system_end_seconds  ),
                                          steady_clock::time_point(steady_start_seconds),
                                          steady_clock::time_point(steady_end_seconds  ));*/

            process_data->sessions.emplace_back(system_clock::time_point(system_start_seconds),
                                                system_clock::time_point(system_end_seconds),
                                                steady_clock::time_point(steady_start_seconds),
                                                steady_clock::time_point(steady_end_seconds));
        } 

        process_data->is_active = (*j)["is_active"];    // NOTE(damian): is a stored process currently active.
        process_data->is_tracked = (*j)["is_tracked"];   // NOTE(damian): this is used to determine, if a process has to have sessions created when declarated inactive.
        process_data->was_updated = (*j)["was_updated"];
    }
    catch (...) {
        return false;
    }
    return true;
}

void convert_to_json(Process_data* process_data, json* j) {
    using std::chrono::duration_cast;
    using std::chrono::seconds;

    (*j)["steady_start"] = duration_cast<seconds>(process_data->steady_start.time_since_epoch()).count(); 
    (*j)["system_start"] = duration_cast<seconds>(process_data->system_start.time_since_epoch()).count();
    
    json j_data;
    convert_to_json(&process_data->data, &j_data);
    (*j)["data"] = j_data;
    
    (*j)["is_active"]   = process_data->is_active;
    (*j)["is_tracked"]  = process_data->is_tracked;
    (*j)["was_updated"] = process_data->was_updated;
    
    vector<json> vec_j_win32_data;
    for (Process_data::Session& session : process_data->sessions) {
        json session_json;
        session_json["steady_start_time"] = duration_cast<seconds>(session.steady_start_time.time_since_epoch()).count();
        session_json["steady_end_time"]   = duration_cast<seconds>(session.steady_end_time.time_since_epoch()).count();

        session_json["system_start_time"] = duration_cast<seconds>(session.system_start_time.time_since_epoch()).count();
        session_json["system_end_time"]   = duration_cast<seconds>(session.system_end_time.time_since_epoch()).count();
        vec_j_win32_data.push_back(session_json);
    }
    (*j)["sessions"] = vec_j_win32_data;
}

