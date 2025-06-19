#include "Process_data.h"

Process_data::Process_data(Win32_process_data win32_data) {
    this->data        = win32_data;

    // TODO(damian): maybe have this as void start_session().
    this->steady_start = std::chrono::steady_clock::now();
    this->system_start = std::chrono::system_clock::now();

    this->is_tracked  = false;
    this->is_active   = false;
    this->was_updated = false;

    Process_data::System_times sys_times = { 0 };
    sys_times.cpu_idle_time   = win32_data.cpu_idle_time;
    sys_times.cpu_kernel_time = win32_data.cpu_kernel_time;
    sys_times.cpu_user_time   = win32_data.cpu_user_time;

    this->sys_times = sys_times;
    this->cpu_usage = std::nullopt;
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
std::pair<bool, Session> Process_data::update_inactive() {
    if (this->is_active == true) {
        this->is_active = false;
        
        if (this->is_tracked) {
            std::chrono::nanoseconds duration = std::chrono::steady_clock::now() - this->steady_start;
            auto duration_sec                 = std::chrono::duration_cast<std::chrono::seconds>(duration);

            auto start_time_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(this->system_start.time_since_epoch());
            auto end_time_in_seconds   = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());

            Session session(duration_sec, start_time_in_seconds, end_time_in_seconds);

            return std::pair(true, session);
        }
    }
    else {
        // Update nothing here
        return std::pair(false, Session());
    }

    this->was_updated = true;
}

void Process_data::update_data(Win32_process_data* new_win32_data) {
    if (this->sys_times.has_value()) { 
        ULONGLONG system_usage = this->sys_times.value().cpu_kernel_time + this->sys_times.value().cpu_user_time;
        this->cpu_usage        = (int) ((system_usage  - this->sys_times.value().cpu_idle_time) * 100) / system_usage;
    }

    Process_data::System_times new_sys_times = {0};
    new_sys_times.cpu_idle_time   = new_win32_data->cpu_idle_time;
    new_sys_times.cpu_kernel_time = new_win32_data->cpu_kernel_time;
    new_sys_times.cpu_user_time   = new_win32_data->cpu_user_time;

    this->sys_times = new_sys_times;
    this->data      = *new_win32_data;
}

bool Process_data::compare(Process_data other) {
    return (   this->data.exe_path      == other.data.exe_path 
            && this->data.creation_time == other.data.creation_time
           ); 
}

bool Process_data::compare_as_tracked(Process_data other) {
    return this->data.exe_path == other.data.exe_path;
}

// == Process_data::Session =================================================================

Session::Session(seconds duration_sec, 
                 seconds system_start_in_seconds, 
                 seconds system_end_in_seconds
) {
    this->duration_sec                 = duration_sec;
    this->system_start_time_in_seconds = system_start_in_seconds;
    this->system_end_time_in_seconds   = system_end_in_seconds;
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
    
        process_data->is_active    = (*j)["is_active"];    // NOTE(damian): is a stored process currently active.
        process_data->is_tracked   = (*j)["is_tracked"];   // NOTE(damian): this is used to determine, if a process has to have sessions created when declarated inactive.
        process_data->was_updated  = (*j)["was_updated"];
        process_data->cpu_usage    = (*j)["cpu_usage"].get<int64_t>();

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
    (*j)["cpu_usage"]   = process_data->cpu_usage.value_or(-1);

}
