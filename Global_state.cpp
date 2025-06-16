// TODO(damian): note why these are here for.
#include <iostream>
#include <fstream>
#include <tuple>
#include <unordered_map>

#include "Functions_win32.h"
#include "Functions_file_system.h"
#include "Global_state.h"

using G_state::Error;
using G_state::Error_type;
using std::unordered_map;

namespace G_state {
    // == Error type inits ============================
    Error::Error(Error_type type) {
        this->type = type;
        this->message = string("");
    }

    Error::Error(Error_type type, const char *message) {
        this->type = type;
        this->message = string(message);
    }

    // ================================================

    // == G_state variables ==========================
    const char* path_file_tracked_processes = "tracked_processes.json";
    const char* path_dir_sessions           = "Sessions_data";
    vector<Process_data> currently_active_processes;
    vector<Process_data> tracked_processes;

    // ================================================

    // == G_state functions ===========================

    static void  convert_path_to_windows_filename(string* path_to_be_changed);
    static Error save_tracked_processes();

    G_state::Error set_up_on_startup() {
                
        std::error_code err_code_1;
        bool data_exists = fs::exists(G_state::path_file_tracked_processes, err_code_1);

        if (err_code_1) {
            return Error(Error_type::filesystem_error, err_code_1.message().c_str());
        }
        if (!data_exists) {
            return Error(Error_type::file_with_tracked_processes_doesnt_exist);
        }


        // Read data
        std::string text;
        if (read_file(G_state::path_file_tracked_processes, &text) != 0) {
            return Error(Error_type::error_reading_a_file);
        }

        json data_as_json;
        try {
            data_as_json = json::parse(text);
        }
        catch(...) {
            return Error(Error_type::json_parsing_failed, text.c_str());
        }

        vector<json> vector_of_j_tracked;
        try { vector_of_j_tracked = data_as_json["process_paths_to_track"]; } 
        catch(...) { return Error(Error_type::json_invalid_strcture); }

        for (json &j_path : vector_of_j_tracked) {
            Process_data new_process(Win32_process_data{0});
            new_process.data.exe_path = j_path; // TODO(damian): check what happends if this is not a string.
            new_process.is_tracked    = true;

            G_state::tracked_processes.push_back(new_process);
        }


        std::error_code err_code_2;
        bool exists = std::filesystem::exists(G_state::path_dir_sessions, err_code_2);

        if (err_code_2) {
            return Error(Error_type::filesystem_error, err_code_2.message().c_str());
        }
        if (!err_code_2) {
            return Error(Error_type::sessions_data_folder_doesnt_exist);
        }

        std::error_code err_code_3;
        bool is_dir = std::filesystem::is_directory(G_state::path_dir_sessions, err_code_3);
        
        if (err_code_3) {
            return Error(Error_type::filesystem_error, err_code_3.message().c_str());
        }
        if (!is_dir) {
            return Error(Error_type::sessions_data_folder_doesnt_exist);
        }

        return Error(Error_type::ok);
    }

    G_state::Error G_state::update_state() {

        pair<vector<Win32_process_data>, Win32_error> result = win32_get_process_data();
        if (result.second != Win32_error::ok) {
            // TODO(damian): handle better.
            assert(false);
        }

        // NOTE(damian): for code clarity.
        for (Process_data &data : G_state::tracked_processes) {
            if (data.was_updated)
                assert(false);
        }
        for (Process_data &data : G_state::currently_active_processes) {
            if (data.was_updated)
                assert(false);
        }

        // Updating the state
        for (Win32_process_data& win32_data : result.first) {

            bool is_tracked = false;
            for (Process_data& g_state_data : G_state::tracked_processes) {
                if (g_state_data.compare_as_tracked(win32_data)) {
                    g_state_data.update_active();
                    g_state_data.update_data(&win32_data);
                    is_tracked = true;

                    // TODO(damian): maybe assert to make sure that there is no other exact process.
                }
            }

            bool was_active_before = false;
            if (!is_tracked) {
                for (Process_data& g_state_data : G_state::currently_active_processes) {
                    if (g_state_data.compare(win32_data)) {
                        g_state_data.update_active();
                        g_state_data.update_data(&win32_data);
                        was_active_before = true;

                        // TODO(damian): maybe assert to make sure that there is no other exact process.
                    }
                }
            }

            if (!was_active_before && !is_tracked) {
                Process_data new_process(win32_data);
                new_process.update_active();
                G_state::currently_active_processes.push_back(new_process);
            }
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

                // NOTE(damian): this path conversion for csv file name is temporary.
                string process_path_copy = process_data.data.exe_path;
                convert_path_to_windows_filename(&process_path_copy);

                std::filesystem::path csv_file_path;
                csv_file_path.append(G_state::path_dir_sessions);
                csv_file_path.append(process_path_copy);
                csv_file_path.replace_extension(".csv"); // NOTE(damian): this is error prone. 

                std::error_code err_code_1;
                bool exists = std::filesystem::exists(csv_file_path, err_code_1);
                
                if (err_code_1) {
                    return Error(Error_type::filesystem_error, err_code_1.message().c_str());
                }
                if (!exists) {
                    return Error(Error_type::no_csv_file_for_tracked_process);
                }

                std::fstream csv_file; // NOTE(damian): std::ios::app also create a file if it doesnt exist, that why i check for exists above.
                csv_file.open(csv_file_path, std::ios::out | std::ios::app);

                if (!csv_file.is_open()) {
                    csv_file.clear(); // Resets the stream
                    csv_file.close();
                    
                    return Error(Error_type::no_csv_file_for_tracked_process, process_path_copy.c_str());
                }

                csv_file << 
                    new_session.duration_sec.count() << ", " << 
                    new_session.system_start_time_in_seconds.count() << ", " << 
                    new_session.system_end_time_in_seconds.count()   << 
                    '\n';
            }
        }

        // Reseting the was_updated bool for future updates.
        for (Process_data& process_data : G_state::currently_active_processes) {
            process_data.was_updated = false;
        }
        for (Process_data& process_data : G_state::tracked_processes) {
            process_data.was_updated = false;
        }

        // G_state::create_tree();

        return Error(Error_type::ok);
    }

    G_state::Error add_process_to_track(string* path) {
        Win32_process_data win32_data = {0};

        // TODO(damian): this is temporaty.
        win32_data.exe_path = *path;

        Process_data new_process(win32_data);

        bool already_tracking = false;
        for (Process_data &process : G_state::tracked_processes) {
            if (process.compare_as_tracked(new_process))
                already_tracking = true;
        }

        if (already_tracking)
            return Error(Error_type::trying_to_track_the_same_process_more_than_once);

        // Checking if it is already being tracked inside the vector active once
        auto p_to_active = G_state::currently_active_processes.begin();
        bool found_active = false;
        for (;
             p_to_active != G_state::currently_active_processes.end();
             ++p_to_active
        ) {
            if (p_to_active->compare_as_tracked(new_process)) {
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
        for (Process_data &tracked : G_state::tracked_processes) {
            for (Process_data &current : G_state::currently_active_processes) {
                if (tracked.compare(current)) { 
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
        string process_path_copy = new_process.data.exe_path;
        convert_path_to_windows_filename(&process_path_copy);

        std::filesystem::path csv_file_path;
        csv_file_path.append(G_state::path_dir_sessions);
        csv_file_path.append(process_path_copy);
        csv_file_path.replace_extension(".csv");

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
            if (p_to_tracked->data.exe_path == *path) {
                is_tracked = true;
                break;
            }
        }

        if (!is_tracked)
            return Error(Error_type::trying_to_untrack_a_non_tracked_process);

        G_state::tracked_processes.erase(p_to_tracked);

        // Need to rewrite the file that stores processes tracked
        std::error_code err_code_1;
        bool data_exists = fs::exists(G_state::path_file_tracked_processes, err_code_1);

        if (err_code_1) {
            return Error(Error_type::filesystem_error, err_code_1.message().c_str());
        }
        if (!data_exists) {
            return Error(Error_type::file_with_tracked_processes_doesnt_exist);
        }

        json j_tracked;
        vector<string> paths;
        for (Process_data& process : G_state::tracked_processes) {
            paths.push_back(process.data.exe_path);            
        }
        j_tracked["process_paths_to_track"] = paths;

        std::ofstream file(G_state::path_file_tracked_processes, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            return Error(Error_type::file_with_tracked_processes_doesnt_exist);
        }
        file << j_tracked.dump(4);
        file.close();

        return Error(Error_type::ok);
    }
   


    static void convert_path_to_windows_filename(string* path_to_be_changed) {
        for (auto it  = path_to_be_changed->begin(); 
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
            return Error(Error_type::filesystem_error, err_code.message().c_str());
        }
        if (!data_exists) {
            return Error(Error_type::file_with_tracked_processes_doesnt_exist);
        }

        json j_tracked;
        vector<string> paths;
        for (Process_data& process : G_state::tracked_processes) {
            paths.push_back(process.data.exe_path);            
        }
        j_tracked["process_paths_to_track"] = paths;

        std::ofstream file(G_state::path_file_tracked_processes, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            return Error(Error_type::file_with_tracked_processes_doesnt_exist);
        }
        file << j_tracked.dump(4);
        file.close();

        return Error(Error_type::ok);
    }























    // ================================================

    static void print_process_data(Process_data* process) {
        std::cout << "(name: " << process->data.exe_name<< ", pid: " << process->data.pid << ", ppid: " << process->data.ppid << ")";
    }

    // void create_tree() {
    //     // Init a tree
    //     unordered_map<DWORD, Node*> tree;

    //     for (Process_data& process : G_state::currently_active_processes) {
    //         Node* new_node = new Node; // TODO(damian): why does malloc then read access violate, but new doesnt?
            
    //         if (new_node == NULL) {
    //             // TODO(damian): handle better.
    //             assert(false);
    //         }

    //         new_node->process               = &process;
    //         new_node->child_processes_nodes = vector<Node*>();

    //         tree[process.data.pid] = new_node;
    //     }
        
    //     // Fill the tree up with current data
    //     for (Process_data& process : G_state::currently_active_processes) {
    //         auto process_node = tree.find(process.data.pid);
    //         if (process_node == tree.end()) {
    //             // TODO(damian): handle better.
    //             assert(false); // This cant happend.
    //         };            

    //         auto parent_node = tree.find(process.data.ppid);
    //         if (parent_node == tree.end()) continue; // Its ok

    //         parent_node->second->child_processes_nodes.push_back(process_node->second);
    //     }
        
    //     // Finding root processes
    //     for (auto& pair : tree) {
    //         Node* node = pair.second;
    //         DWORD ppid = node->process->data.ppid;

    //         auto it = tree.find(ppid);
    //         if (it == tree.end()) {// Not found
    //             G_state::roots_for_process_tree.push_back(node);
    //         }
    //     }
        
    // }

    
}
