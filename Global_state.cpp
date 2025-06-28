#include <iostream>
#include <fstream>
#include <tuple>
#include <unordered_map>
#include <ctime>
#include <iomanip>

#include "Functions_win32.h"
#include "Functions_file_system.h"
#include "Global_state.h"

using G_state::Error;
using G_state::Error_type;
using std::unordered_map;

// NOTE(damian): this is fowrard declaration of the Client namespace from Handlers_for_main.
namespace Client {
    struct Data_thread_error_status {
        bool handled;
        G_state::Error error;
    };
    extern std::list<Data_thread_error_status> data_thread_error_queue;
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
    const char* path_file_error_logs        = "Error_logs.txt"; 
    const char* path_file_tracked_processes = "tracked_processes.json";
    const char* path_dir_sessions           = "Sessions_data";
    const char* path_dir_process_icons      = "Process_icons";
    const char* path_file_settings          = "Settings.json";

    const char* process_session_csv_file_header = "duration_in_seconds, start_time_in_seconds_since_unix_epoch, end_time_in_seconds_since_unix_epoch \n";
    
    const char* csv_file_name_for_overall_sessions_for_process = "history.csv";
	const char* csv_file_name_for_current_session_for_process  = "current_session.csv";
    // ================================================

	// == Data data ==============================================
    vector<Process_data> currently_active_processes;
    vector<Process_data> tracked_processes;
    // ================================================
    static Error write_tracked_to_json_file();

    static Error write_new_session_to_csv    (Session* session, Process_data* process);
    static Error write_current_session_to_csv(Session* session, Process_data* process);
    static Error clear_current_session_csv   (Process_data* process);
    static Error handle_files_for_add_tracked(Process_data* process);
    
    static Error log_error(const char* err_message) {
        // Getting time for the log message.
        auto   now  = std::chrono::system_clock::now();
        time_t time = std::chrono::system_clock::to_time_t(now);
        tm     tm;
        int err_time = localtime_s(&tm, &time); 

        // Checking if the error logs file exists.
        std::error_code err_1;
        bool logs_exist = fs::exists(G_state::path_file_error_logs, err_1);
        if (err_1) { return Error(Error_type::os_error); }
        if (!logs_exist) {
            std::fstream file(G_state::path_file_error_logs, std::ios::out);
            if (!file.is_open()) {
                return Error(Error_type::os_error);
            }

            if (err_time == 0) {
                file << std::put_time(&tm, "%Y %B %d (%A), (%H:%M) ");
            } else {
                file << "[TIME_ERROR] ";
            }
            file << "File for error logs didnt exists as was expected.\n";
            file.close();
        }
        
        std::fstream file(G_state::path_file_error_logs, std::ios::in | std::ios::app);
        if (!file.is_open()) {
            return Error(Error_type::os_error);
        }
        if (err_time == 0) {
            file << std::put_time(&tm, "%Y %B %d (%A), (%H:%M) ");
        } else {
            file << "[TIME_ERROR] ";
        }
        file << err_message << "\n";
        file.close();

        Client::Data_thread_error_status new_err_status = {false, Error(Error_type::err_logs_file_was_not_present)};
        Client::data_thread_error_queue.push_back(new_err_status);
    
        return Error(Error_type::ok);
    }

    // TODO: do we need to log fatal errors? 

    G_state::Error set_up_on_startup() {
        // == Setting up the icons folder.
        {
            std::error_code err_code;
            bool icons_dir_newly_created = fs::create_directory(G_state::path_dir_process_icons, err_code);
            if (err_code) { return Error(Error_type::os_error); }
            if (icons_dir_newly_created) {
                Error err_on_log = log_error("Folder with icons for processes wasnt present when was expected. New one was created.");
                if (err_on_log.type != Error_type::ok) { return err_on_log; }
    
                Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_folder_for_process_icons_doesnt_exist)};
                Client::data_thread_error_queue.push_back(new_err_status);
            }
        }

        // == Checking if the file with tracked processes exists.
        {
            std::error_code err_code;
            bool tracked_exist = fs::is_regular_file(G_state::path_file_tracked_processes, err_code); 
            if (!tracked_exist) {
                if (err_code != std::errc::no_such_file_or_directory) { return Error(Error_type::os_error); }
                else { 
                    std::fstream file(G_state::path_file_tracked_processes, std::ios::out);
                    file.close();
                    
                    Error err = write_tracked_to_json_file();
                    if (err.type != Error_type::ok) { return err; }
                    
                    Error err_on_log = log_error("File with tracked processes was not present on startup. New one was created.");
                    if (err_on_log.type != Error_type::ok) { return err_on_log; }
                    
                    Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_file_with_tracked_processes_doesnt_exist)};
                    Client::data_thread_error_queue.push_back(new_err_status);
                }
            }
            
        }

        // == Storing data from tracked processes file.
        {
            std::error_code err_code;
            bool exists = fs::is_regular_file(G_state::path_file_tracked_processes, err_code);
            if (!exists)  { 
                if (err_code != std::errc::no_such_file_or_directory) { return Error(Error_type::os_error); }    
                else { 
                    return Error(Error_type::startup_filesystem_is_all_fucked_up); 
                    // NOTE(damian): this is the case, since we were supposed to create this file before this.
                }
            } 

            std::fstream file(G_state::path_file_tracked_processes, std::ios::in);
            if (!file.is_open()) { return Error(Error_type::os_error); };
            
            string text, line;
            while (std::getline(file, line)) {
                text.append(line);
            }   
            file.close();

            json data_as_json;
            try { data_as_json = json::parse(text); }
            catch (...) {
                Error err_on_log = log_error("Error when parsing json file with tracked processes on startup. GG. App cant run if this is the case.");
                if (err_on_log.type != Error_type::ok) { return err_on_log; }

                Error err(Error_type::startup_json_tracked_processes_file_parsing_failed);
                return err;
            }

            if (!data_as_json.contains("tracked_processes_paths")) {
                Error err_on_log = log_error("Invalid structure for file with tracked processes on startu. GG. App cant run if this is the case.");
                if (err_on_log.type != Error_type::ok) { return err_on_log; }

                Error err(Error_type::startup_json_tracked_processes_file_invalid_structure);
                return err;
            }

            if (!data_as_json["tracked_processes_paths"].is_array()) {
                Error err_on_log = log_error("Error when reading file with tracked processes, its values were of invalid type. GG. App cant run if this is the case.");
                if (err_on_log.type != Error_type::ok) { return err_on_log; }

                Error err(Error_type::startup_invalid_values_inside_json);
                return err;
            }

            vector<json> vector_of_j_tracked;
            vector_of_j_tracked = data_as_json["tracked_processes_paths"];

            for (json& j_path : vector_of_j_tracked) {
                if (!j_path.is_string()) {
                    Error err_on_log = log_error("Error when reading tracked processes json, its value were of a different type from ones that were expected. GG. App cant run if this is the case.");
                    if (err_on_log.type != Error_type::ok) { return err_on_log; }

                    Error err(Error_type::startup_invalid_values_inside_json);
                    return err;
                }
                else {
                    Process_data new_process(j_path.get<std::string>()); 
                    new_process.is_tracked = true;
        
                    G_state::tracked_processes.push_back(new_process);
                }
            }
        }

        // == Doing stuff with folder for sessions and nested folders/files.
        {
            // Checking sessions folder.
            std::error_code err_1;
            bool newly_created = fs::create_directories(G_state::path_dir_sessions, err_1);
            if (err_1) { return Error(Error_type::os_error); }
            if (newly_created) {
                Error err_on_log = log_error("A folder for sessions was not pressesnt on startup, a new one was created.");
                if (err_on_log.type != Error_type::ok) { return err_on_log; }

                Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_sessions_folder_doesnt_exist)};
                Client::data_thread_error_queue.push_back(new_err_status);
            }

            // Checking process session folders and nested csv files for each tracked process.
            for (Process_data& process : G_state::tracked_processes) {
                fs::path path;

                path.append(G_state::path_dir_sessions);
                string process_path_copy = process.exe_path;
                convert_path_to_windows_filename(&process_path_copy);
                path.append(std::move(process_path_copy));
                
                // Process sessions folder
                std::error_code err_2;
                bool newly_created = fs::create_directories(path, err_2);
                if (err_2) { return Error(Error_type::os_error); }
                if (newly_created) {
                    Error err_on_log = log_error("Sessions directory for a specific tracked process was not present at startup. New one was created.");
                    if (err_on_log.type != Error_type::ok) { return err_on_log; }

                    Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_no_dir_for_specific_tracked_process)};
                    Client::data_thread_error_queue.push_back(new_err_status);
                }

                // CSV overall file
                fs::path overall_sessions_path = path;
                overall_sessions_path.append(G_state::csv_file_name_for_overall_sessions_for_process);
                
                std::error_code err_3;
                bool overall_exists = fs::is_regular_file(overall_sessions_path, err_3);
                if (!overall_exists) {
                    if (err_3 != std::errc::no_such_file_or_directory) { return Error(Error_type::os_error); }
                    else {
                        std::fstream file(overall_sessions_path, std::ios::out);
                        if (!file.is_open()) { return Error(Error_type::os_error); }
                        else {
                            file << G_state::process_session_csv_file_header;
                            file.close();

                            Error err_on_log = log_error("No sessions history csv file for a process was found on startup. New one was created.");
                            if (err_on_log.type != Error_type::ok) { return err_on_log; }

                            Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_no_overall_csv_file_for_tracked_process)};
                            Client::data_thread_error_queue.push_back(new_err_status);
                        }
                    }
                }

                // CSV current files
                fs::path current_sessions_path = path;
                current_sessions_path.append(G_state::csv_file_name_for_current_session_for_process);
                
                std::error_code err_4;
                bool current_exists = fs::is_regular_file(current_sessions_path, err_4);
                if (!current_exists) {
                    if (err_4 != std::errc::no_such_file_or_directory) { return Error(Error_type::os_error); }
                    else {
                        std::fstream file(current_sessions_path, std::ios::out);
                        if (!file.is_open()) { return Error(Error_type::os_error); }
                        else {
                            file << G_state::process_session_csv_file_header;
                            file.close();

                            Error err_on_log = log_error("No current session csv file for a process was found on startup. New one was created.");
                            if (err_on_log.type != Error_type::ok) { return err_on_log; }

                            Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_no_current_session_csv_file_for_tracked_process)};
                            Client::data_thread_error_queue.push_back(new_err_status);
                        }
                    }
                }
                else { // Current file existed, check if it has some data from prev run. If it does, write it to overall csv and clear the csv.
                    std::fstream csv_file(current_sessions_path, std::ios::in | std::ios::out);
                    if (!csv_file.is_open()) { return Error(Error_type::os_error); }
                    
                    int line_count = 0;
                    string csv_line;
                    while (std::getline(csv_file, csv_line)) {
                        line_count += 1;
                        if (line_count == 1) { continue; } // CSV header.
                        else if (line_count == 2) {
                            // Write to overall csv.
                            std::fstream overall_csv_file(overall_sessions_path, std::ios::app);
                            if (!overall_csv_file.is_open()) { return Error(Error_type::os_error); }

                            overall_csv_file << csv_line << "\n";
                            overall_csv_file.close();

                            Error err = clear_current_session_csv(&process);
                            if (err.type != Error_type::ok) { return err; }
                        }
                        else { return Error(Error_type::process_current_csv_file_contains_more_than_1_record); }
                    } 
                      
                }

            }
        }

        // == Reading data from the file with settigs.
        {
            std::error_code err_code;
            bool settings_exist = fs::is_regular_file(G_state::path_file_settings, err_code);
            if (!settings_exist) {
                if (err_code != std::errc::no_such_file_or_directory) { return Error(Error_type::os_error); }
                else {
                    json j_settings;
                    j_settings["data_thread_update_time_seconds"] = Settings::n_sec_between_state_updates;
                    
                    std::fstream file(G_state::path_file_settings, std::ios::out);
                    file << j_settings.dump(4);
                    file.close();

                    Error err_on_log = log_error("Was not able to read settings from file on startup. A new one was created.");
                    if (err_on_log.type != Error_type::ok) { return err_on_log; }

                    Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_setting_file_doesnt_exists)};
                    Client::data_thread_error_queue.push_back(new_err_status);
                }
            }

            std::fstream file(G_state::path_file_settings, std::ios::in);
            if (!file.is_open()) { return Error(Error_type::os_error); };

            string text, line;
            while (std::getline(file, line)) {
                text.append(line);
            }
            file.close();   

            json j_settings;
            try { j_settings = json::parse(text); }
            catch (...) {
                Error err_on_log = log_error("Error when parsing json file with settings on startup. GG. App cant run if this is the case.");
                if (err_on_log.type != Error_type::ok) { return err_on_log; }

                return Error(Error_type::startup_json_tracked_processes_file_parsing_failed);
            }

            if (!j_settings.contains("data_thread_update_time_seconds")) {
                Error err_on_log = log_error("Invalid structure for settings file on startup. GG. App cant run if this is the case.");
                if (err_on_log.type != Error_type::ok) { return err_on_log; }

                Error err(Error_type::startup_json_settings_file_invalid_structure);
                return err;
            }

            if (j_settings["data_thread_update_time_seconds"].is_number_unsigned()) {
                Settings::n_sec_between_state_updates = j_settings["data_thread_update_time_seconds"]; 
            }
            else {
                Error err_on_log = log_error("Error when reading settings json, its values were of invalid type. GG. App cant run if this is the case.");
                if (err_on_log.type != Error_type::ok) { return err_on_log; }

                Error err(Error_type::startup_invalid_values_inside_json);
                return err;
            }
        }

        // Setting up static system data
        // ...

        return Error(Error_type::ok);
    }

    G_state::Error update_state() {
        auto result = win32_get_process_data();
        G_state::Error               error         = std::get<0>(result);
        vector<Win32_process_data>   processes     = std::get<1>(result);
        optional<Win32_system_times> new_sys_times = std::get<2>(result);
        Dynamic_system_info::up_time               = std::get<3>(result);
        Dynamic_system_info::system_time           = std::get<4>(result);
        if (error.type != Error_type::ok) { return error; }

        G_state::Dynamic_system_info::prev_system_times = G_state::Dynamic_system_info::new_system_times;
        G_state::Dynamic_system_info::new_system_times  = new_sys_times;

        // NOTE(damian): this was comented out before official first version on 28th of June 2025.
        // for (Process_data& data : G_state::tracked_processes) {
        //     if (data.was_updated)
        //         assert(false);
        // }
        // for (Process_data& data : G_state::currently_active_processes) {
        //     if (data.was_updated)
        //         assert(false);
        // }

        // Updating the state
        for (Win32_process_data& win32_data : processes) {
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
                    
                    // NOTE(damian): cant have 2 same processes stores as tracked.
                    // if (is_tracked) { return Error(Error_type::runtime_logics_failed); } 

                    pair<Error, optional<Session>> result = g_state_data.update_active();
                    if (result.first.type != Error_type::ok) { 
                        return result.first; 
                    }

                    if (result.second.has_value()) {
                        if (result.second.value().duration_sec.count() > 0) {
                            Error err = write_current_session_to_csv(&result.second.value(), &g_state_data);
                            if (err.type != Error_type::ok) { 
                                return err; 
                            }
                        }
                    }

                    Error err = g_state_data.update_data(&win32_data);
                    if (err.type != Error_type::ok) { 
                        return err; 
                    }
                    
                    is_tracked = true;
                    break; 
                }
            }
            if (is_tracked) continue;
            
            bool was_active_before = false;
            for (Process_data& g_state_data : G_state::currently_active_processes) {

                pair<Error, bool> result = g_state_data.compare(&win32_data);
                if (result.first.type != Error_type::ok) {
                    return result.first; 
                }

                if (!g_state_data.was_updated && result.second) { // NOTE(damian): leaving !g_state_data.was_updated, in case 2 chrome like processes have same spawn time.
                    
                    // NOTE(damian): cant have 2 same active processes
                    // if (was_active_before) { return Error(Error_type::runtime_logics_failed); } 

                    pair<Error, optional<Session>> update_result = g_state_data.update_active();
                    if (update_result.first.type != Error_type::ok) { 
                        return update_result.first;
                    }
                    // Ignoring the possible session here. Dont need it for active, non tracked processes.

                    Error err = g_state_data.update_data(&win32_data);
                    if (err.type != Error_type::ok) { 
                        return err; 
                    }
                    
                    was_active_before = true;
                    break; 
                }
            }
            if (was_active_before) continue;

            Process_data new_process(&win32_data);
            pair<Error, optional<Session>> update_result = new_process.update_active();
            if (update_result.first.type != Error_type::ok) {
                return update_result.first; 
            }
            // Ignoring the possible session here. Dont need it for active, non tracked processes.
            
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

        // Updating inactive tracked processes
        for (Process_data& process_data : G_state::tracked_processes) {
            if (!process_data.was_updated) {
                pair<Error, optional<Session>> result = process_data.update_inactive();
                if (result.first.type != Error_type::ok) { return result.first; }

                if (!result.second.has_value()) {  continue; }
                // No session was created after the update. (The process has been inactive for a while now)
                
                process_data.reset_data();

                Session new_session = result.second.value();

                Error err_1 = write_new_session_to_csv(&new_session, &process_data);
                if (err_1.type != Error_type::ok) { return err_1; }

                Error err_2 = clear_current_session_csv(&process_data);
                if (err_2.type != Error_type::ok) { return err_2; }
            }
        }

        // Reseting the was_updated bool for future updates.
        for (Process_data& process_data : G_state::currently_active_processes) {
            process_data.was_updated = false;
        }
        for (Process_data& process_data : G_state::tracked_processes) {
            process_data.was_updated = false;
        }

        // Creating data for the client.
        if (G_state::Client_data::need_data) {
            G_state::Client_data::data.copy_currently_active_processes = G_state::currently_active_processes; 
            G_state::Client_data::data.copy_tracked_processes          = G_state::tracked_processes;

            // Telling the client thread that data it requested is ready
            G_state::Client_data::need_data  = false;
        }

        return Error(Error_type::ok);
    }

    G_state::Error add_process_to_track(string* path) {
        Win32_process_data win32_data = { 0 };
        win32_data.exe_path = *path;

        Process_data new_process(&win32_data);

        // Checking if the process is alredy being handled
        bool already_tracking = false;
        for (Process_data& process : G_state::tracked_processes) {
            if (process.compare_as_tracked(&new_process)) {
                // if (already_tracking) { return Error(Error_type::runtime_logics_failed); }
                already_tracking = true;
                break;
            }
        }

        if (already_tracking) {
            Error err(Error_type::trying_to_track_the_same_process_more_than_once);
            Client::Data_thread_error_status new_err_status = {false, err};
            Client::data_thread_error_queue.push_back(new_err_status);
        }
        else {
            // Checking if it is already inside the vector active once
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
                G_state::tracked_processes.push_back(std::move(*p_to_active)); // TODO(damian): check if move actually does anything here.
                G_state::currently_active_processes.erase(p_to_active);
            }
            else { // Just adding to tracked
                new_process.is_tracked = true;
                G_state::tracked_processes.push_back(new_process);
            }

            // NOTE(damian): this is a sneaky way to check all processes in a O(n^2) loop. )).
            //               its fine here, since how often will someone actually add processes. 
            for (Process_data& tracked : G_state::tracked_processes) {
                for (Process_data& current : G_state::currently_active_processes) {
                    if (tracked.times.has_value()) { // It might not have yet been active. And compare() needs it to have value.
                        pair<Error, bool> result = tracked.compare(&current);
                        if (result.first.type != Error_type::ok) { return result.first; }

                        if (result.second) { return Error(Error_type::tracked_and_current_process_vectors_share_data); }
                    }
                    
                }
            }

            Error err = handle_files_for_add_tracked(&new_process);
            if (err.type != Error_type::ok) { return err; }
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
            Client::Data_thread_error_status new_err_status = {false, err};
            Client::data_thread_error_queue.push_back(new_err_status);
        }
        else {
            G_state::tracked_processes.erase(p_to_tracked);

            std::error_code err_code_1;
            bool data_exists = fs::exists(G_state::path_file_tracked_processes, err_code_1);
            if (err_code_1) { return Error(Error_type::os_error); }
            if (!data_exists) {
                return Error(Error_type::runtime_filesystem_is_all_fucked_up);
            }

            Error err = write_tracked_to_json_file();
            if (err.type != Error_type::ok) { return err; }
        }

        return Error(Error_type::ok);
    }

    G_state::Error update_settings_file(uint32_t n_sec) {
        Settings::n_sec_between_state_updates = n_sec;

        std::error_code err_code;
        bool exists = fs::is_regular_file(G_state::path_file_settings, err_code);
        if (!exists) {
            if (err_code != std::errc::no_such_file_or_directory) { return Error(Error_type::os_error); }
            else { 
                return Error(Error_type::runtime_filesystem_is_all_fucked_up); 
            }
        }
        
        // TODO(damian): use some constants for these json keys, shit is getting ridiculous.
        json j_settings;
        j_settings["data_thread_update_time_seconds"] = Settings::n_sec_between_state_updates;
        
        std::fstream file(G_state::path_file_settings, std::ios::in | std::ios::out);
        if (!file.is_open()) { return Error(Error_type::os_error); }
        file << j_settings.dump(4);
        file.close();

        return Error(Error_type::ok);
    }

    void convert_path_to_windows_filename(string* path_to_be_changed) {
        for (auto it = path_to_be_changed->begin();
            it != path_to_be_changed->end();
            ++it
        ) {
            if (*it == '\\') *it = '-';
            if (*it == ':')  *it = '~';
        }
    }
    // ================================================

    // == Private helpers =============================
    static Error write_tracked_to_json_file() {
        std::error_code err_code;
        bool file_exists = fs::is_regular_file(G_state::path_file_tracked_processes, err_code);
        if (!file_exists) {
            if (err_code != std::errc::no_such_file_or_directory) { return Error(Error_type::os_error); }
            else {
                return Error(Error_type::runtime_filesystem_is_all_fucked_up);
            }
        }
        
        vector<string> j_tracked;
        for (Process_data& process : G_state::tracked_processes) {
            j_tracked.push_back(process.exe_path);
        }
        
        json j;
        j["tracked_processes_paths"] = j_tracked;
        
        std::fstream file(G_state::path_file_tracked_processes, std::ios::in | std::ios::out);
        if (!file.is_open()) { return Error(Error_type::os_error); }
        file << j.dump(4);
        file.close();

        return Error(Error_type::ok);
    }

    static Error write_new_session_to_csv(Session* session, Process_data* process) {
        fs::path path;
        
        string process_path_copy = process->exe_path;
        convert_path_to_windows_filename(&process_path_copy);

        path.append(G_state::path_dir_sessions);
        path.append(process_path_copy);
        path.append(G_state::csv_file_name_for_overall_sessions_for_process);

        std::error_code err;
        bool overall_exists = fs::is_regular_file(path, err);
        if (!overall_exists) {
            if (err != std::errc::no_such_file_or_directory) { return Error(Error_type::os_error); }
            else {
                return Error(Error_type::runtime_filesystem_is_all_fucked_up);
            }
        }
        
        std::fstream csv_overall_file(path, std::ios::app);
        if (!csv_overall_file.is_open()) { return Error(Error_type::os_error); }
        
        csv_overall_file <<
            session->duration_sec.count() << ", " <<
            session->system_start_time_in_seconds.count() << ", " <<
            session->system_end_time_in_seconds.count() <<
            '\n';
        csv_overall_file.close();

        return Error(Error_type::ok);
    } 

    static Error write_current_session_to_csv(Session* session, Process_data* process) {
        fs::path path;
        
        string process_path_copy = process->exe_path;
        convert_path_to_windows_filename(&process_path_copy);

        path.append(G_state::path_dir_sessions);
        path.append(process_path_copy);
        path.append(G_state::csv_file_name_for_current_session_for_process);

        std::error_code err;
        bool current_exists = fs::is_regular_file(path, err);
        if (!current_exists) {
            if (err != std::errc::no_such_file_or_directory) { return Error(Error_type::os_error); }
            else {
                return Error(Error_type::runtime_filesystem_is_all_fucked_up);
            }
        }
        
        std::fstream csv_current_file(path, std::ios::out | std::ios::trunc);
        if (!csv_current_file.is_open()) { return Error(Error_type::os_error); }
        
        csv_current_file << G_state::process_session_csv_file_header;
        csv_current_file <<
            session->duration_sec.count() << ", " <<
            session->system_start_time_in_seconds.count() << ", " <<
            session->system_end_time_in_seconds.count() <<
            '\n';
        csv_current_file.close();

        return Error(Error_type::ok);
    }

    static Error clear_current_session_csv(Process_data* process) {
        fs::path path;
        
        string process_path_copy = process->exe_path;
        convert_path_to_windows_filename(&process_path_copy);

        path.append(G_state::path_dir_sessions);
        path.append(process_path_copy);
        path.append(G_state::csv_file_name_for_current_session_for_process);

        std::error_code err;
        bool current_exists = fs::is_regular_file(path, err);
        if (!current_exists) {
            if (err != std::errc::no_such_file_or_directory) { return Error(Error_type::os_error); }
            else { return Error(Error_type::runtime_filesystem_is_all_fucked_up); }
        }

        std::fstream csv_current_file(path, std::ios::out | std::ios::trunc);
        if (!csv_current_file.is_open()) { return Error(Error_type::os_error); }
        csv_current_file << G_state::process_session_csv_file_header;
        csv_current_file.close();

        return Error(Error_type::ok);
    }

    static Error handle_files_for_add_tracked(Process_data* process) {
        // Storirng tracked to file. 
        {
            Error err = write_tracked_to_json_file();
            if (err.type != Error_type::ok) { return err; } 
        }

        // Checking sessions dir folder.
        {
            std::error_code err_code;
            bool newly_created = fs::create_directory(G_state::path_dir_sessions, err_code);
            if (err_code)      { return Error(Error_type::os_error); }
            if (newly_created) { return Error(Error_type::runtime_filesystem_is_all_fucked_up); } 
        }

        // Checking sessions folder specific for this process.
        {
            fs::path path;
            
            path.append(G_state::path_dir_sessions);
            string process_path_copy = process->exe_path;
            convert_path_to_windows_filename(&process_path_copy);
            path.append(process_path_copy); 
            
            std::error_code err_1;
            bool new_process_specific_dir = fs::create_directories(path, err_1);
            if (err_1) { return Error(Error_type::os_error); }
            
            fs::path path_overall = path;
            path_overall.append(G_state::csv_file_name_for_overall_sessions_for_process);
    
            fs::path path_current = path;
            path_current.append(G_state::csv_file_name_for_current_session_for_process);

            if (new_process_specific_dir) {
                // Creating history csv file
                std::fstream file_overall(path_overall, std::ios::out);
                if (file_overall.is_open()) {
                    file_overall << G_state::process_session_csv_file_header;
                    file_overall.close();
                }
                else { return Error(Error_type::os_error); }

                // Creation current session csv file
                std::fstream file_current(path_current, std::ios::out);
                if (file_current.is_open()) {
                    file_current << G_state::process_session_csv_file_header;
                    file_current.close();
                }
                else { return Error(Error_type::os_error); }
            }
            else {
                // Checking existance of history csv file.
                std::error_code err_1;
                bool exists_overall = fs::exists(path_overall, err_1);
                if (err_1)           { return Error(Error_type::os_error); }
                if (!exists_overall) { return Error(Error_type::runtime_filesystem_is_all_fucked_up); }
                
                // Checking existance of current session csv file.
                std::error_code err_2;
                bool exists_current = fs::exists(path_current, err_2);
                if (err_2)           { return Error(Error_type::os_error); }
                if (!exists_current) { return Error(Error_type::runtime_filesystem_is_all_fucked_up); }
            }
        }

        return Error(Error_type::ok);
    }

    // ================================================

    namespace Client_data {
        bool need_data;
		Data data;
    }

    namespace Static_system_info {
        
    }

    namespace Dynamic_system_info {
        long long up_time      = 0;
        SYSTEMTIME system_time = {0};

        optional<Win32_system_times> prev_system_times;
        optional<Win32_system_times> new_system_times;
    }

    namespace Settings {
        const uint32_t default_n_sec_between_state_updates = 3;
        uint32_t       n_sec_between_state_updates         = default_n_sec_between_state_updates;

        const uint32_t default_n_sec_between_csv_updates = 3;
		uint32_t       n_sec_between_csv_updates         = default_n_sec_between_csv_updates; 
	}




















    // ================================================

    
}