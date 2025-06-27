#include "Process_data.h"
#include "Functions_win32.h"     // For Win32_process_data definition

#include "Global_state.h"

Process_data::Process_data(string exe_path) {
    this->is_active    = false;    
    this->is_tracked   = false;   
    this->was_updated  = false;  

    this->steady_start = std::nullopt;
    this->system_start = std::nullopt;

    this->last_time_session_was_created = std::nullopt;

    this->snapshot       = std::nullopt;
    this->product_name   = std::nullopt;
    this->priority_class = std::nullopt;
    this->process_times  = {0};             // TODO(damian): maybe make this null also, but win32 data cant have it as null. This being null with be great for inactive tracked processes.
    this->affinities     = std::nullopt;
    this->ram_usage      = std::nullopt;
    this->has_image      = false;

    this->exe_path     = exe_path;
    this->times        = std::nullopt;
    this->cpu_usage    = std::nullopt;
}

Process_data::Process_data(Win32_process_data* win32_data) {
    this->is_tracked   = false;
    this->is_active    = false;
    this->was_updated  = false;
    
    this->steady_start = std::nullopt;
    this->system_start = std::nullopt;

    this->last_time_session_was_created = std::nullopt;

    this->snapshot       = win32_data->snapshot;
    this->product_name   = win32_data->product_name;
    this->priority_class = win32_data->priority_class;
    this->process_times  = win32_data->process_times;
    this->affinities     = win32_data->affinities;
    this->ram_usage      = win32_data->ram_usage;
    this->has_image      = win32_data->has_image;

    this->exe_path  = win32_data->exe_path;
    this->times     = win32_data->process_times;
    this->cpu_usage = std::nullopt;
}

static Session create_session(Process_data* process) {
    if (   !process->steady_start.has_value()
        || !process->system_start.has_value()
    ) {
        assert(false);
    }

    std::chrono::nanoseconds duration = std::chrono::steady_clock::now() - process->steady_start.value();
    auto duration_sec = std::chrono::duration_cast<std::chrono::seconds>(duration);

    auto start_time_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(process->system_start.value().time_since_epoch());
    auto end_time_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());

    Session session(duration_sec, start_time_in_seconds, end_time_in_seconds);

    return session;
}

// Updating a process as if its new state is active
std::pair<bool, Session> Process_data::update_active() {
    this->was_updated = true;

    if (this->is_active == false) {
        this->is_active    = true;
        this->steady_start = std::chrono::steady_clock::now();
        this->system_start = std::chrono::system_clock::now();

        this->last_time_session_was_created = std::chrono::steady_clock::now();

        if (this->is_tracked) {
            Session session = create_session(this);
            return std::pair(true, session);
        }
    } 
    else {
        if (this->last_time_session_was_created.has_value()) {
            auto duration_ns = std::chrono::steady_clock::now().time_since_epoch() - this->last_time_session_was_created.value().time_since_epoch();
            auto duration_s  = std::chrono::duration_cast<std::chrono::seconds>(duration_ns);

            if (this->is_tracked && duration_s.count() > G_state::Settings::n_sec_between_csv_updates) {
                this->last_time_session_was_created = std::chrono::steady_clock::now();

                Session session = create_session(this);
                return std::pair(true, session);
            }
        }
    }
    return std::pair(false, Session());
}

// Updating a process as if its new state is inactive
std::pair<bool, Session> Process_data::update_inactive() {
    this->was_updated = true;
    
    if (this->is_active == true) {
        this->is_active = false;
        
        if (this->is_tracked) {
            if (   !this->steady_start.has_value() 
                || !this->system_start.has_value() 
            ) {
                assert(false);
            }

            Session session = create_session(this);

            return std::pair(true, session);
        }
    }
    else {
        // Nothing here
    }

    return std::pair(false, Session());
}

void Process_data::update_data(Win32_process_data* new_win32_data) {
    assert(this->exe_path == new_win32_data->exe_path);
    
    this->snapshot       = new_win32_data->snapshot;
    this->product_name   = new_win32_data->product_name;
    this->priority_class = new_win32_data->priority_class;
    this->process_times  = new_win32_data->process_times;
    this->affinities     = new_win32_data->affinities;
    this->ram_usage      = new_win32_data->ram_usage;
    this->has_image      = new_win32_data->has_image;

    this->exe_path = new_win32_data->exe_path;
    
    // Getting CPU.
    namespace dyn_inf = G_state::Dynamic_system_info;
    if (   this->times.has_value()
        // && new_win32_data->process_times.has_value()
        && dyn_inf::prev_system_times.has_value()
        && dyn_inf::new_system_times.has_value()
    ) {
        ULONGLONG prev_process_time    = this->times.value().kernel_time + this->times.value().user_time;
        ULONGLONG current_process_time = new_win32_data->process_times.kernel_time + new_win32_data->process_times.user_time;
        ULONGLONG delta_process_time   = current_process_time - prev_process_time;
        
        // NOTE(damian): idle time is included into kernel for system times, so have to remove it.
        ULONGLONG prev_system_time    = (dyn_inf::prev_system_times.value().kernel_time - dyn_inf::prev_system_times.value().idle_time) + dyn_inf::prev_system_times.value().user_time;
        ULONGLONG current_system_time = (dyn_inf::new_system_times.value().kernel_time - dyn_inf::new_system_times.value().idle_time) + dyn_inf::new_system_times.value().user_time;
        ULONGLONG delta_system_time   = current_system_time - prev_system_time;

        if (delta_system_time == 0) {
            assert(false); // TODO(damian): deal with this.
        }
        
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        DWORD n_cores = sys_info.dwNumberOfProcessors; // TODO(damian): store this inside static system data.
        
        this->cpu_usage = ((double) delta_process_time / (delta_system_time * n_cores)) * 100;
    }
    // ====
    
    this->times = new_win32_data->process_times;
}

bool Process_data::compare(Process_data* other) {
    if (   !this->times.has_value()
        || !other->times.has_value() 
    ) {
        // TODO(damian): handle better.
        assert(false);
    }

    return (   this->exe_path == other->exe_path 
            && this->times.value().creation_time == other->times.value().creation_time
           ); 
}

bool Process_data::compare(Win32_process_data* data) {
    if (!this->times.has_value()) {
        assert(false);
    }

    // THIS IS WHY, NOTE IT FOR THE FUTURE
    return (   this->exe_path == data->exe_path 
            && this->times.value().creation_time == data->process_times.creation_time
           ); 
}

bool Process_data::compare_as_tracked(Process_data* other) {
    return this->exe_path == other->exe_path;
}

bool Process_data::compare_as_tracked(Win32_process_data* data) {
    return string(this->exe_path) == string(data->exe_path);
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

void convert_to_json(Process_data* process_data, json* j) {
    using std::chrono::duration_cast;
    using std::chrono::seconds;

    (*j)["is_active"]   = process_data->is_active;
    (*j)["is_tracked"]  = process_data->is_tracked;
    (*j)["was_updated"] = process_data->was_updated;

    if (process_data->steady_start.has_value()) {
        (*j)["steady_start"] = duration_cast<seconds>(process_data->steady_start.value().time_since_epoch()).count(); 
    } 
    else { 
        (*j)["steady_start"] = nullptr;
    }

    if (process_data->steady_start.has_value()) {
        (*j)["system_start"] = duration_cast<seconds>(process_data->system_start.value().time_since_epoch()).count(); 
    } 
    else { 
        (*j)["system_start"] = nullptr; 
    }

    if (process_data->product_name.has_value()) {
        (*j)["product_name"] = process_data->product_name.value();
    }
    else {
        (*j)["product_name"] = nullptr;
    }

    if (process_data->snapshot.has_value()) {
        (*j)["pid"] = process_data->snapshot.value().pid;
        (*j)["started_threads"] = process_data->snapshot.value().started_threads;
        (*j)["base_priority"] = process_data->snapshot.value().base_priority;
        (*j)["exe_name"] = process_data->snapshot.value().exe_name;
    } else {
        (*j)["pid"] = nullptr;
        (*j)["started_threads"] = nullptr;
        (*j)["base_priority"] = nullptr;
        (*j)["exe_name"] = nullptr;
    }

    (*j)["exe_path"] = process_data->exe_path;

    // Times data
    if (process_data->times.has_value()) {
        (*j)["creation_time"] = process_data->times.value().creation_time;
        (*j)["exit_time"] = process_data->times.value().exit_time;
        (*j)["kernel_time"] = process_data->times.value().kernel_time;
        (*j)["user_time"] = process_data->times.value().user_time;
    } else {
        (*j)["creation_time"] = nullptr;
        (*j)["exit_time"] = nullptr;
        (*j)["kernel_time"] = nullptr;
        (*j)["user_time"] = nullptr;
    }

    // RAM usage
    if (process_data->ram_usage.has_value()) {
        (*j)["ram_usage"] = process_data->ram_usage.value();
    } else {
        (*j)["ram_usage"] = nullptr;
    }

    // CPU usage
    if (process_data->cpu_usage.has_value()) {
        (*j)["cpu_usage"] = process_data->cpu_usage.value();
    } else {
        (*j)["cpu_usage"] = nullptr;
    }
}
