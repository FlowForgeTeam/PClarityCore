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
    const char* path_file_error_logs        = "Error_logs.txt"; // TODO(damian): create on start up and warn if this was for some reason not created already.
    const char* path_file_tracked_processes = "tracked_processes.json";
    const char* path_dir_sessions           = "Sessions_data";

    const char* process_session_csv_file_header = "duration_in_seconds, start_time_in_seconds_since_unix_epoch, end_time_in_seconds_since_unix_epoch \n";
    
    const char* csv_file_name_for_overall_sessions_for_process = "history";
	const char* csv_file_name_for_current_session_for_process  = "current_session";
    // ================================================

	// == Data data ==============================================
    vector<Process_data> currently_active_processes;
    vector<Process_data> tracked_processes;
    // ================================================
    
    static void convert_path_to_windows_filename(string* path_to_be_changed);
    
    static Error write_new_session_to_csv(Session* session, Process_data* process) {
        string process_path_copy = process->exe_path;
        convert_path_to_windows_filename(&process_path_copy);

        fs::path process_sessions_dir_path;
        process_sessions_dir_path.append(G_state::path_dir_sessions);
        process_sessions_dir_path.append(process_path_copy);

        // Creating a file for history of sessions.
        fs::path path_csv_overall = process_sessions_dir_path;
        path_csv_overall.append(G_state::csv_file_name_for_overall_sessions_for_process);
        path_csv_overall.replace_extension(".csv");

        std::error_code err_1;
        bool overall_exists = fs::exists(path_csv_overall, err_1);
        if (err_1) { return Error(Error_type::os_error); }
        if (!overall_exists) {
            return Error(Error_type::startup_no_overall_csv_file_for_tracked_process);
        }

        std::fstream csv_overall_file(path_csv_overall, std::ios::app);
        if (!csv_overall_file.is_open()) { return Error(Error_type::os_error); }
        csv_overall_file <<
            session->duration_sec.count() << ", " <<
            session->system_start_time_in_seconds.count() << ", " <<
            session->system_end_time_in_seconds.count() <<
            '\n';
        csv_overall_file.close();
    } 

    static Error handle_files_for_add_tracked(json* j_tracked, Process_data* process) {
        // Checking if runtime file tree is ok
        std::error_code err_1;
        bool exists = fs::exists(G_state::path_file_tracked_processes, err_1);
        if (err_1) { return Error(Error_type::os_error); }
        if (!exists) {
            return Error(Error_type::startup_file_with_tracked_processes_doesnt_exist);
        }

        std::fstream file_tracked(G_state::path_file_tracked_processes, std::ios::out);
        if (file_tracked.is_open()) {
            file_tracked << j_tracked->dump(4);
            file_tracked.close();
        } 
        else { return Error(Error_type::os_error); }

        // Creating a sessions folder for this process
        string process_path_copy = process->exe_path;
        convert_path_to_windows_filename(&process_path_copy);

        fs::path path;
        path.append(G_state::path_dir_sessions);

        // Checking sessions dir
        std::error_code err_2;
        bool sessions_dir_exists = fs::exists(path, err_2);
        if (err_2) { return Error(Error_type::os_error); }
        if (!sessions_dir_exists) { return Error(Error_type::runtime_filesystem_is_all_fucked_up); } 

        // Checking process specific sessions folder
        path.append(std::move(process_path_copy));
        std::error_code err_3;
        bool new_process_specific_dir = fs::create_directories(path, err_3);
        if (err_3) { return Error(Error_type::os_error); }

        fs::path path_overall = path;
        path_overall.append(G_state::csv_file_name_for_overall_sessions_for_process);
        path_overall.replace_extension(".csv");

        fs::path path_current = path;
        path_current.append(G_state::csv_file_name_for_current_session_for_process);
        path_current.replace_extension(".csv");

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
            std::error_code err_overall;
            bool exists_overall = fs::exists(path_overall, err_overall);
            if (err_overall) {return Error(Error_type::os_error); }
            if (!exists_overall) {
                return Error(Error_type::runtime_filesystem_is_all_fucked_up);
            }
            
            // Checking existance of current session csv file.
            std::error_code err_current;
            bool exists_current = fs::exists(path_current, err_current);
            if (err_current) {return Error(Error_type::os_error); }
            if (!exists_current) {
                return Error(Error_type::runtime_filesystem_is_all_fucked_up);
            }

        } 
    }

    G_state::Error set_up_on_startup() {
        // Preparing string for error log file.
        // auto   now  = std::chrono::system_clock::now();
        // time_t time = std::chrono::system_clock::to_time_t(now);
        // tm     tm;
        // int err_time = localtime_s(&tm, &time); 
        // const char* log_message = (err_time == 0 ? "" : "");
        // std::put_time(&tm, "%Y %B %d (%A), (%H:%M)");

        // Checking if the error logs file exists.
        std::error_code err_1;
        bool logs_exist = fs::exists(G_state::path_file_error_logs, err_1);
        if (err_1) { return Error(Error_type::os_error); }
        if (!logs_exist) {
            std::fstream file(G_state::path_file_error_logs, std::ios::out);
            if (!file.is_open()) {
                return Error(Error_type::os_error);
            }
            else {
                // TODO(damian): white to logs.
                Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_err_logs_file_was_not_present)};
                Client::data_thread_error_queue.push_back(new_err_status);
            }
        }

        // Checking if the file with tracked processes exists.
        std::error_code err_2;
        bool tracked_exist = fs::exists(G_state::path_file_tracked_processes, err_2);
        if (err_2) { return Error(Error_type::os_error); }
        if (!tracked_exist) {
            std::fstream file(G_state::path_file_tracked_processes, std::ios::out);
            if (!file.is_open()) {
                return Error(Error_type::os_error);
            }
            else {
                json j;
                j["process_paths_to_track"] = json::array();
                file << j.dump(4);
                file.close();

                // TODO(damian): white to logs.
                Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_file_with_tracked_processes_doesnt_exist)};
                Client::data_thread_error_queue.push_back(new_err_status);
            }
        }

        // Storing data from tracked processes file
        std::string text;
        if (read_file(G_state::path_file_tracked_processes, &text) != 0) {
            return Error(Error_type::os_error);
        }

        json data_as_json;
        try { data_as_json = json::parse(text); }
        catch (...) {
            Error err(Error_type::startup_json_tracked_processes_file_parsing_failed, text.c_str());
            return err;
        }

        vector<json> vector_of_j_tracked;
        try { vector_of_j_tracked = data_as_json["process_paths_to_track"]; }
        catch (...) { 
            Error err(Error_type::startup_json_tracked_processes_file_invalid_structure);
            return err;
        }

        for (json& j_path : vector_of_j_tracked) {
            if (!j_path.is_string()) {
                Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_invalid_values_inside_json)};
                Client::data_thread_error_queue.push_back(new_err_status);
            }
            else {
                Process_data new_process(j_path);
                new_process.exe_path   = j_path; // TODO(damian): check what happends if this is not a string.
                new_process.is_tracked = true;
    
                G_state::tracked_processes.push_back(new_process);
            }
        }

        // Checking if folder for sessions exists.
        std::error_code err_3;
        bool newly_created = fs::create_directories(G_state::path_dir_sessions, err_3);
        if (err_3) { return Error(Error_type::os_error); }
        if (!newly_created) {
            // TODO(damian): white to logs.
            Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_sessions_folder_doesnt_exist)};
            Client::data_thread_error_queue.push_back(new_err_status);
        }

        // Checking session folder and nested csb files for each tracked process
        for (Process_data& process : G_state::tracked_processes) {
            fs::path path;
            path.append(G_state::path_dir_sessions);

            string process_path_copy = process.exe_path;
            convert_path_to_windows_filename(&process_path_copy);
            path.append(std::move(process_path_copy));
            
            // Folder
            std::error_code err_4;
            bool newly_created = fs::create_directories(path, err_4);
            if (err_4) { return Error(Error_type::os_error); }
            if (!newly_created) {
                // TODO(damian): white to logs.
                Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_no_dir_for_specific_tracked_process)};
                Client::data_thread_error_queue.push_back(new_err_status);
            }

            // CSV overall file
            fs::path overall_sessions_path = path;
            overall_sessions_path.append(G_state::csv_file_name_for_overall_sessions_for_process);
            overall_sessions_path.replace_extension(".csv");
            
            std::error_code err_5;
            bool overall_exists = fs::exists(overall_sessions_path, err_5);
            if (err_5) { return Error(Error_type::os_error); }
            if (!overall_exists) {
                std::fstream file(overall_sessions_path, std::ios::out);
                if (!file.is_open()) { return Error(Error_type::os_error); }
                else {
                    file << G_state::process_session_csv_file_header;
                    file.close();

                    // TODO(damian): white to logs.
                    Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_no_overall_csv_file_for_tracked_process)};
                    Client::data_thread_error_queue.push_back(new_err_status);
                }
            }

            // CSV current files
            fs::path current_sessions_path = path;
            current_sessions_path.append(G_state::csv_file_name_for_current_session_for_process);
            current_sessions_path.replace_extension(".csv");
            
            std::error_code err_6;
            bool current_exists = fs::exists(current_sessions_path, err_6);
            if (err_6) { return Error(Error_type::os_error); }
            if (!current_exists) {
                std::fstream file(current_sessions_path, std::ios::out);
                if (!file.is_open()) { return Error(Error_type::os_error); }
                else {
                    file << G_state::process_session_csv_file_header;
                    file.close();

                    // TODO(damian): white to logs.
                    Client::Data_thread_error_status new_err_status = {false, Error(Error_type::startup_no_current_session_csv_file_for_tracked_process)};
                    Client::data_thread_error_queue.push_back(new_err_status);
                }
            }

        }

        return Error(Error_type::ok);
    }

    G_state::Error update_state() {

        tuple< vector<Win32_process_data>, 
               optional<Win32_system_times> > result = win32_get_process_data();
        vector<Win32_process_data>   processes    = std::get<0>(result);
        optional<Win32_system_times> system_times = std::get<1>(result);

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
                    assert(!is_tracked); // NOTE(damian): cant have 2 same processes stores as tracked.

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

        // Updating inactive tracked processes
        for (Process_data& process_data : G_state::tracked_processes) {
            if (!process_data.was_updated) {
                std::pair<bool, Session> maybe_new_session = process_data.update_inactive();

                if (!maybe_new_session.first) { // No session was created after the update. (The process didnt just become inactive)
                    continue;
                }

                Session new_session = maybe_new_session.second;

                string process_path_copy = process_data.exe_path;
                convert_path_to_windows_filename(&process_path_copy);

                fs::path process_sessions_dir_path;
                process_sessions_dir_path.append(G_state::path_dir_sessions);
                process_sessions_dir_path.append(process_path_copy);

                // Creating a file for history of sessions.
                fs::path path_csv_overall = process_sessions_dir_path;
                path_csv_overall.append(G_state::csv_file_name_for_overall_sessions_for_process);
                path_csv_overall.replace_extension(".csv");

                std::error_code err_1;
                bool overall_exists = fs::exists(path_csv_overall, err_1);
                if (err_1) { return Error(Error_type::os_error); }
                if (!overall_exists) {
                    return Error(Error_type::startup_no_overall_csv_file_for_tracked_process);
                }

                std::fstream csv_overall_file(path_csv_overall, std::ios::app);
                if (!csv_overall_file.is_open()) { return Error(Error_type::os_error); }
                csv_overall_file <<
                    new_session.duration_sec.count() << ", " <<
                    new_session.system_start_time_in_seconds.count() << ", " <<
                    new_session.system_end_time_in_seconds.count() <<
                    '\n';
                csv_overall_file.close();
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

            data.copy_currently_active_processes = G_state::currently_active_processes; 
            data.copy_tracked_processes          = G_state::tracked_processes;

            // Telling the client thread that data it requested is ready
            G_state::Client_data::maybe_data = data;
            G_state::Client_data::need_data  = false;
        }

        return Error(Error_type::ok);
    }

    G_state::Error add_process_to_track(string* path) {
        Win32_process_data win32_data = { 0 };
        win32_data.exe_path = *path;

        Process_data new_process(&win32_data);

        // Checking if the process is a;redy being handled
        bool already_tracking = false;
        for (Process_data& process : G_state::tracked_processes) {
            if (process.compare_as_tracked(&new_process)) {
                assert(!already_tracking); // This was not suppodef to happend in the first place.
                already_tracking = true;
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

            // Creating json for tracked
            json j_tracked;
            vector<string> paths;		

            for (Process_data& process : G_state::tracked_processes) {
                paths.push_back(process.exe_path);
            }
            j_tracked["process_paths_to_track"] = paths;

            handle_files_for_add_tracked(&j_tracked, &new_process);

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

            // Need to rewrite the file that stores processes tracked
            std::error_code err_code_1;
            bool data_exists = fs::exists(G_state::path_file_tracked_processes, err_code_1);
            if (err_code_1) { return Error(Error_type::os_error); }
            if (!data_exists) {
                return Error(Error_type::runtime_filesystem_is_all_fucked_up);
            }

            json j_tracked;
            vector<string> paths;
            for (Process_data& process : G_state::tracked_processes) {
                paths.push_back(process.exe_path);
            }
            j_tracked["process_paths_to_track"] = paths;

            std::fstream file(G_state::path_file_tracked_processes, std::ios::out | std::ios::trunc);
            if (!file.is_open()) { return Error(Error_type::os_error); }
            file << j_tracked.dump(4);
            file.close();
        }

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