// TODO(damian): note why these are here for.
#include <iostream>
#include <fstream>
#include <tuple>
#include <unordered_map>
#include <ctime>
#include <iomanip>

#include "Functions_win32.h"
#include "Functions_file_system.h"
#include "Global_state.h"

// #include "Handlers_for_main.h"

using G_state::Error;
using G_state::Error_type;
using std::unordered_map;

namespace Client {
    struct Data_thread_error_status {
        bool handled;
        G_state::Error error;
    };
    std::list<Data_thread_error_status> data_thread_error_queue;
}


namespace G_state {
    // == Error type inits ============================
    Error::Error(Error_type type) {
        this->type = type;
        this->message = string("");
    }

    Error::Error(Error_type type, const char* message) {
        this->type = type;
        this->message = string(message);
    }

    // ================================================

	// == Constants ===================================
    const char* path_file_error_logs        = "Error_logs.txt"; // TODO(damian): create on start up and warn if this was for some reason not created already.
    const char* path_file_tracked_processes = "tracked_processes.json";
    const char* path_dir_sessions           = "Sessions_data";
    // ================================================

	// == Data data ==============================================
    vector<Process_data> currently_active_processes;
    vector<Process_data> tracked_processes;
    // ================================================

    static void  convert_path_to_windows_filename(string* path_to_be_changed);
    static Error save_tracked_processes();

    static bool handle_error(Error* error) {

        if ((int) error->type < 10) return false;
        
        if ((int) error->type < 100) { // Warning

            bool logs_file_was_not_pressent = false;
            std::error_code err_1;
            bool exists = std::filesystem::exists(G_state::path_file_error_logs, err_1);
            if (err_1) {
                // TODO(damian): handle.
                assert(false);
            }
            if (!exists) {
                std::fstream file(G_state::path_file_error_logs, std::ios::out);
                logs_file_was_not_pressent = true;

                if (file.is_open()) {
                    file.close();
                } else {
                    assert(false);
                    // TODO: handle.
                }
            }

            std::fstream logs_file(G_state::path_file_error_logs, std::ios::out | std::ios::app);
            if (!logs_file.is_open()) {
                // TODO(damian): handle.
                assert(false);
            }

            auto   now  = std::chrono::system_clock::now();
            time_t time = std::chrono::system_clock::to_time_t(now);
            tm tm;
            int err_time = localtime_s(&tm, &time); 
            if (err_time != 0) {
                logs_file << "error_converting_time" << "\n";
            }
            else {
                logs_file << std::put_time(&tm, "%Y %B %d (%A), (%H:%M)")
                      << "\t" << error->message 
                      << "\n";
            }
            logs_file.close();
        
            if(logs_file_was_not_pressent) {
                Client::Data_thread_error_status new_status{false, Error(Error_type::err_logs_file_was_not_present)};
                Client::data_thread_error_queue.push_back(new_status);
            }

            Client::Data_thread_error_status new_status{false, *error};
            Client::data_thread_error_queue.push_back(new_status);

            switch(error->type) {
                case Error_type::file_with_tracked_processes_doesnt_exist: {
                    std::fstream file(G_state::path_file_tracked_processes, std::ios::out);
    
                    if (file.is_open()) {
                        json empty_json = json::object();
                        file << empty_json.dump(4);
                        file.close();
                    } else {
                        assert(false);
                        // TODO: handle.
                    }
                } break;

                case Error_type::sessions_data_folder_doesnt_exist: {
                    if (std::filesystem::create_directory(G_state::path_dir_sessions)) {} 
                    else {
                        assert(false);
                        // TODO: handle
                    }
                } break;

                case Error_type::trying_to_track_the_same_process_more_than_once: {
                    // Nothing here, we just notify the client
                } break;

                case Error_type::trying_to_untrack_a_non_tracked_process: {
                    // Nothing here, we just notify the client
                } break;

                case Error_type::no_csv_file_for_tracked_process: {
                    // TODO: handle.
                } break;

                default: {
                    // TODO(damian): handle better, probablt a fatal error ))
                    assert(false);
                } break;
            }

            return false;
        }
        else {
            // Nothing to do here, everything is all fucked up alredy.

            // TODO(damian): complete the fatal err handeling.
            return true;
        }
    }

    G_state::Error set_up_on_startup() {

        std::error_code err_code_1;
        bool data_exists = fs::exists(G_state::path_file_tracked_processes, err_code_1);

        if (err_code_1) {
            Error err(Error_type::filesystem_error, err_code_1.message().c_str());
            if(handle_error(&err)) { return err; }
        }
        if (!data_exists) {
            Error err(Error_type::file_with_tracked_processes_doesnt_exist);
            if(handle_error(&err)) { return err; }
        }

        // Read data
        std::string text;
        if (read_file(G_state::path_file_tracked_processes, &text) != 0) {
            // Error err(Error_type::error_reading_a_file);
            // if(handle_error(&err)) { return err; }

            // TODO(damian): need to do.
            assert(false);
        }

        json data_as_json;
        try {
            data_as_json = json::parse(text);
        }
        catch (...) {
            Error err(Error_type::startup_json_tracked_processes_file_parsing_failed, text.c_str());
            if(handle_error(&err)) { return err; }
        }

        vector<json> vector_of_j_tracked;
        try { vector_of_j_tracked = data_as_json["process_paths_to_track"]; }
        catch (...) { return Error(Error_type::startup_json_tracked_processes_file_invalid_structure); }

        for (json& j_path : vector_of_j_tracked) {
            Process_data new_process(j_path);
            new_process.exe_path   = j_path; // TODO(damian): check what happends if this is not a string.
            new_process.is_tracked = true;

            G_state::tracked_processes.push_back(new_process);
        }

        std::error_code err_code_2;
        bool exists = std::filesystem::exists(G_state::path_dir_sessions, err_code_2);

        if (err_code_2) {
            assert(false);
            // TODO: handle better.
        }
        if (!exists) {
            Error err(Error_type::sessions_data_folder_doesnt_exist, "Dir with session was not present when it was supposed to be.");
            if(handle_error(&err)) { return err; }
        }

        return Error(Error_type::ok);
    }

    G_state::Error update_state() {

        pair<vector<Win32_process_data>, Win32_error> result = win32_get_process_data();
        if (result.second != Win32_error::ok) {
            // TODO(damian): handle better.
            assert(false);
        }

        // NOTE(damian): for code clarity.
        for (Process_data& data : G_state::tracked_processes) {
            if (data.was_updated)
                assert(false);
        }
        for (Process_data& data : G_state::currently_active_processes) {
            if (data.was_updated)
                assert(false);
        }

        // Updating the state
        for (Win32_process_data& win32_data : result.first) {
            // NOTE(damian): since thracked work like tracked (They are compared only by path).
            //               we might have chrome like processes that spawn 10 chromes. 
            //               since we iterate over the win32 getched current processes, 
            //               when we iterate with the first chrome and chrome is tracked, we will update the tracked one.
            //               All the other ones have to go into currently_acrive_processes vector. 
            //               To do so, we have to skip the tracked process that have alredy been updated.
            //               This way the second chrome wont update the single, alredy updated tracked chrome data,
            //                  but inted will go to the cur active and then compare itseld to the very presise other chrome data, 
            //                  since curr active are compared via path and nano-second creation time, so there wont be a time when there are 2 same chromes in the cur_active.
            bool is_tracked = false;
            for (Process_data& g_state_data : G_state::tracked_processes) {
                if (!g_state_data.was_updated && g_state_data.compare_as_tracked(&win32_data)) {
                    assert(!is_tracked); // NOTE(damian): cant have 2 same processes stores as tracked.

                    if (g_state_data.data.has_value()) {
                        if (g_state_data.data.value().exe_name == "Telegram.exe") {
                            int x = 2;
                        }
                    }

                    g_state_data.update_active();
                    g_state_data.update_data(&win32_data);
                    is_tracked = true;

                    // break; // NOTE(damian): commented out for the assert to work while developing.
                }
            }
            if (is_tracked) continue;
            
            // TODO(damian): since we use move for updates, cheking if it is still valid would be nice.

            bool was_active_before = false;
            for (Process_data& g_state_data : G_state::currently_active_processes) {
                if (!g_state_data.was_updated && g_state_data.compare(&win32_data)) { // NOTE(damian): leaving !g_state_data.was_updated, in case 2 chrome like processes have same spawn time.
                    assert(!was_active_before); // NOTE(damian): cant have 2 same active processes 
                    if (was_active_before) {
                        assert(!g_state_data.was_updated); // TODO(damian): delete later.
                    }

                    g_state_data.update_active();
                    g_state_data.update_data(&win32_data);
                    was_active_before = true;

                    // break; // NOTE(damian): commented out for the assert to work while developing.

                    // TODO(damian): maybe assert to make sure that there is no other exact process.
                }
            }
            if (was_active_before) continue;

            Process_data new_process(&win32_data);
            new_process.update_active();
            G_state::currently_active_processes.push_back(new_process);
        }

        // Removing the processes that are not tracked and stopped being active
        for (auto it = G_state::currently_active_processes.begin();
            it != G_state::currently_active_processes.end();
            ) {
            if (!it->was_updated) {
                it = G_state::currently_active_processes.erase(it);
            }
            else {
                ++it;
            }
        }
        
        float total_cpu = 0.0;
        
        for (Process_data& data : G_state::currently_active_processes) {
            if (data.data.value().exe_name == "Telegram.exe") {
                int x = 2;
            }

            total_cpu += data.cpu_usage.value_or(0);
        }
        for (Process_data& data : G_state::tracked_processes) {
            total_cpu += data.cpu_usage.value_or(0);
        }
        if (total_cpu == 0) {
            int x = 2;
        }

        for (Process_data& data : G_state::currently_active_processes) {
            if (data.data.value().exe_name == "chrome.exe") {
                int x = 2;
            }
            total_cpu += data.cpu_usage.value_or(0);
        }
        for (Process_data& data : G_state::tracked_processes) {
            total_cpu += data.cpu_usage.value_or(0);
        }

        std::cout << "Total cpu usage: " << total_cpu << std::endl;

        // Updating inactive tracked processes
        for (Process_data& process_data : G_state::tracked_processes) {
            if (!process_data.was_updated) {
                std::pair<bool, Session> maybe_new_session = process_data.update_inactive();

                if (!maybe_new_session.first) { // No session was created after the update. (The process didnt just become inactive)
                    continue;
                }

                Session new_session = maybe_new_session.second;

                std::error_code err_code_1;
                bool exists_dir = std::filesystem::exists(G_state::path_dir_sessions);
                if (err_code_1) {
                    assert(false);
                    // TODO(damian): handle better.
                    return Error(Error_type::filesystem_error, err_code_1.message().c_str());
                }
                if (!exists_dir) {
                    Error err(Error_type::sessions_data_folder_doesnt_exist, "Dir with sessions data was not pressesnt when it was supposed to be.");
                    if (handle_error(&err)) { return err; }
                }


                string process_path_copy = process_data.exe_path;
                convert_path_to_windows_filename(&process_path_copy);

                std::filesystem::path csv_file_path;
                csv_file_path.append(G_state::path_dir_sessions);
                csv_file_path.append(process_path_copy);
                csv_file_path.replace_extension(".csv"); // NOTE(damian): this is error prone. 

                std::error_code err_code_2;
                bool exists = std::filesystem::exists(csv_file_path, err_code_2);

                if (err_code_2) {
                    assert(false);
                    // TODO(damian): handle better.
                    return Error(Error_type::filesystem_error, err_code_2.message().c_str());
                }
                if (!exists) {
                    Error err(Error_type::no_csv_file_for_tracked_process, "CSV file was not present when a session was created. This was not suppose to happen.");
                    if(handle_error(&err)) { return err; }
                }

                // TODO(damian): file is created here regardless of wheather it existed, this has to change.

                std::fstream csv_file; // NOTE(damian): std::ios::app also create a file if it doesnt exist, that why i check for exists above.
                csv_file.open(csv_file_path, std::ios::out | std::ios::app);

                if (!csv_file.is_open()) {
                    csv_file.clear(); // Resets the stream
                    csv_file.close();

                    assert(false);
                    // TODO: handle better.
                    
                    return Error(Error_type::no_csv_file_for_tracked_process, process_path_copy.c_str());
                }

                if (!exists) {
                    csv_file << "duration_in_seconds, start_time_in_seconds_since_unix_epoch, end_time_in_seconds_since_unix_epoch" << "\n";
                }

                csv_file <<
                    new_session.duration_sec.count() << ", " <<
                    new_session.system_start_time_in_seconds.count() << ", " <<
                    new_session.system_end_time_in_seconds.count() <<
                    '\n';

                csv_file.close();
            }
        }

        // Reseting the was_updated bool for future updates.
        for (Process_data& process_data : G_state::currently_active_processes) {
            process_data.was_updated = false;
        }
        for (Process_data& process_data : G_state::tracked_processes) {
            process_data.was_updated = false;
        }
    
        // Creating data for the clint.

        if (G_state::Client_data::need_data) {

            G_state::Client_data::Data data = {};

            // Storing current active processes for client to use
            // NOTE(damian): move is fine here, update_state() will add all of them back on the next iteration. 
            size_t prev_size_active              = G_state::currently_active_processes.size();
            data.copy_currently_active_processes = G_state::currently_active_processes; // move(G_state::currently_active_processes);

            /*G_state::currently_active_processes = vector<Process_data>();
            G_state::currently_active_processes.reserve(prev_size_active);*/

            // Storing a copy of tracked processes for client to use
            // NOTE(damian): move isnt used here, since tracked are not added nor removed inside update_state(),
            //               we only add processes on startup and when user decided to add/remove a process from being tracked.
            //               update_state() just updates the state (is_active --> true / false)
            data.copy_tracked_processes = G_state::tracked_processes;

            // Telling the client thread that data for it is ready
            G_state::Client_data::maybe_data = data;
            G_state::Client_data::need_data  = false;
        }

        return Error(Error_type::ok);
    }

    G_state::Error add_process_to_track(string* path) {
        Win32_process_data win32_data = { 0 };

        // TODO(damian): this is temporaty.
        win32_data.exe_path = *path;

        Process_data new_process(&win32_data);

        bool already_tracking = false;
        for (Process_data& process : G_state::tracked_processes) {
            if (process.compare_as_tracked(&new_process))
                already_tracking = true;
        }

        if (already_tracking) {
            Error err(Error_type::trying_to_track_the_same_process_more_than_once);
            if(handle_error(&err)) { return err; }

            return Error(Error_type::trying_to_track_the_same_process_more_than_once);
        }

        // Checking if it is already being tracked inside the vector active once
        auto p_to_active = G_state::currently_active_processes.begin();
        bool found_active = false;
        for (;
            p_to_active != G_state::currently_active_processes.end();
            ++p_to_active
            ) {
            if (p_to_active->compare_as_tracked(&new_process)) {
                found_active = true;
                break;
            }
        }

        // Moving from cur_active to tracked
        if (found_active) {
            p_to_active->is_tracked = true;
            G_state::tracked_processes.push_back(std::move(*p_to_active));
            G_state::currently_active_processes.erase(p_to_active);
        }
        else { // Just adding to tracked
            new_process.is_tracked = true;
            G_state::tracked_processes.push_back(new_process);
        }

        // TODO(damian): this is here now for clarity, move somewhere else.
        for (Process_data& tracked : G_state::tracked_processes) {
            for (Process_data& current : G_state::currently_active_processes) {
                if (tracked.compare(&current)) {
                    // NOTE(damian): not comaring as tracked, 
                    //               since then it would return Error for processes like chrome and vs code.
                    return Error(Error_type::tracked_and_current_process_vectors_share_data);
                }
            }
        }

        // Since list of tracked changed, changing the file that stores path for tracked processes
        Error err_code_1 = save_tracked_processes();
        if (err_code_1.type != Error_type::ok) return err_code_1;

        // Creating a csv file with Session history for the process
        string process_path_copy = new_process.exe_path;
        convert_path_to_windows_filename(&process_path_copy);

        std::filesystem::path csv_file_path;
        csv_file_path.append(G_state::path_dir_sessions);
        csv_file_path.append(process_path_copy);
        csv_file_path.replace_extension(".csv");

        // TODO(damian): handle existasnce of session dir here.

        std::error_code err_code_2;
        bool exists = std::filesystem::exists(csv_file_path, err_code_2);

        if (err_code_2) {
            return Error(Error_type::filesystem_error);
        }

        if (!exists) {
            std::ofstream new_file(csv_file_path);
            new_file << "duration_in_seconds, start_time_in_seconds_since_unix_epoch, end_time_in_seconds_since_unix_epoch" << "\n";
            new_file.close();
        }

        return Error(Error_type::ok);
    }

    G_state::Error remove_process_from_track(string* path) {
        bool is_tracked = false;
        auto p_to_tracked = G_state::tracked_processes.begin();
        for (;
            p_to_tracked != G_state::tracked_processes.end();
            ++p_to_tracked
            ) {
            
            if (p_to_tracked->exe_path == *path) {
                is_tracked = true;
                break;
            }
        }

        if (!is_tracked) {
            Error err(Error_type::trying_to_untrack_a_non_tracked_process);
            if(handle_error(&err)) { return err; }
        }

        G_state::tracked_processes.erase(p_to_tracked);

        // Need to rewrite the file that stores processes tracked
        std::error_code err_code_1;
        bool data_exists = fs::exists(G_state::path_file_tracked_processes, err_code_1);

        if (err_code_1) {
            assert(false);
            // TODO: handle

            return Error(Error_type::filesystem_error, err_code_1.message().c_str());
        }
        if (!data_exists) {
            Error err(Error_type::file_with_tracked_processes_doesnt_exist, "File with tracked processes was not present when was expected.");
            if(handle_error(&err)) { return err; }
        }

        json j_tracked;
        vector<string> paths;
        for (Process_data& process : G_state::tracked_processes) {
            paths.push_back(process.exe_path);
        }
        j_tracked["process_paths_to_track"] = paths;

        std::ofstream file(G_state::path_file_tracked_processes, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            assert(false);
            // TODO: handle

        }
        file << j_tracked.dump(4);
        file.close();

        return Error(Error_type::ok);
    }

    // == Private helper functrions ==================

    static void convert_path_to_windows_filename(string* path_to_be_changed) {
        for (auto it = path_to_be_changed->begin();
            it != path_to_be_changed->end();
            ++it
            ) {
            if (*it == '\\') *it = '-';
            if (*it == ':')  *it = '~';
        }
    }

    static Error save_tracked_processes() {
        std::error_code err_code;
        bool data_exists = fs::exists(G_state::path_file_tracked_processes, err_code);

        if (err_code) {
            assert(false);
            // TODO: handle

            return Error(Error_type::filesystem_error, err_code.message().c_str());
        }
        if (!data_exists) {
            Error err(Error_type::file_with_tracked_processes_doesnt_exist, "File with tracked processes was not present when it was supposed to be.");
            if(handle_error(&err)) { return err; }
        }

        json j_tracked;
        vector<string> paths;
        for (Process_data& process : G_state::tracked_processes) {
            paths.push_back(process.exe_path);
        }
        j_tracked["process_paths_to_track"] = paths;

        std::ofstream file(G_state::path_file_tracked_processes, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            assert(false);
            // TODO: handle

            // return Error(Error_type::file_with_tracked_processes_doesnt_exist);
        }
        file << j_tracked.dump(4);
        file.close();

        return Error(Error_type::ok);
    }

    // ================================================

    namespace Client_data {
        bool need_data;
		optional<Data> maybe_data;
    }

    namespace System_info {
        long long up_time      = 0;
        SYSTEMTIME system_time = {0};
    }




















    // ================================================

    
}