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
    const char* file_path_with_tracked_processes = "tracked_processes.json";
    const char* sessions_dir_path                = "Sessions_data";
    vector<Process_data> currently_active_processes;
    vector<Process_data> tracked_processes;
    // ================================================

    // == G_state functions ===========================
    G_state::Error set_up_on_startup() {
        std::error_code err_code_1;
        bool data_exists = fs::exists(G_state::file_path_with_tracked_processes, err_code_1);

        if (err_code_1) {
            return Error(Error_type::filesystem_error, err_code_1.message().c_str());
        }

        if (!data_exists) {
            return Error(Error_type::file_with_tracked_processes_doesnt_exist);
            /*std::ofstream new_file(G_state::data_file_path);
            new_file << "{ \"processes_to_track\": [] }" << std::endl;
            new_file.close();*/
        }

        // Read data
        std::string text;
        if (read_file(G_state::file_path_with_tracked_processes, &text) != 0) {
            return Error(Error_type::error_reading_a_file);
        }

        json data_as_json;
        try {
            data_as_json = json::parse(text);
        }
        catch(...) {
            return Error(Error_type::json_parsing_failed, text.c_str());
        }

        // Parse data, since this is start up, take time and examine json for structural errors
        vector<json> vector_of_j_tracked;
        try { vector_of_j_tracked = data_as_json["process_paths_to_track"]; } catch(...) { return Error(Error_type::json_invalid_strcture); }

        for (json &j_path : vector_of_j_tracked) {
            // Process_data process_data(Win32_process_data{0});
            // if (!convert_from_json(&process_data, &j_process)) {
            //     return Error(Error_type::json_invalid_strcture, text.c_str());
            // };

            // process_data.is_tracked = true;

            Process_data new_process(Win32_process_data{0});
            new_process.data.exe_path = j_path; // TODO(damian): check what happends if this is not a string.
            new_process.is_tracked    = true;

            G_state::tracked_processes.push_back(new_process);
        }

        // Making sure the folder with data for sessions exist.
        std::error_code err_code;
        
        bool exists = std::filesystem::exists(G_state::sessions_dir_path, err_code);

        if (err_code) {
            return Error(Error_type::filesystem_error, err_code.message().c_str());
        }
        if (!exists) {
            return Error(Error_type::sessions_data_folder_doesnt_exist);
        }

        bool is_dir = std::filesystem::is_directory(G_state::sessions_dir_path, err_code);

        if (err_code) {
            return Error(Error_type::filesystem_error, err_code.message().c_str());
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

            // NOTE(damian): for manual hand testing )).
            if (win32_data.exe_name == "Telegram.exe") {
                int x = 2;
            }

            bool is_tracked = false;
            for (Process_data& g_state_data : G_state::tracked_processes) {
                if (g_state_data.data.exe_name == "Telegram.exe") {
                    int x = 2;
                }
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
            it != G_state::currently_active_processes.end();)
        {
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

                if (!maybe_new_session.first) { // No session was created after the update. (The process is just not acttive)
                    break;
                }

                Session new_session = maybe_new_session.second;

                // Get the file for the process this session was created for
                // Convert this session into a cmd line
                // White the new cmd line into the opened file
                // Close the file

                // NOTE(damian): this path conversion for csv file name is temporary.
                string process_path_copy = process_data.data.exe_path;
                for (char& ch : process_path_copy) {
                    if (ch == '\\') ch = '-';
                    if (ch == ':')  ch = '~';
                }

                std::filesystem::path csv_file_path;
                csv_file_path.append(G_state::sessions_dir_path);
                csv_file_path.append(process_path_copy);

                std::fstream csv_file;
                csv_file.open(csv_file_path, std::ios::out | std::ios::app);

                if (!csv_file.is_open()) {
                    csv_file.clear(); // Resets the stream
                    csv_file.close();
                    
                    return Error(Error_type::no_csv_file_for_tracked_process, process_path_copy.c_str());
                }

                csv_file << 
                    new_session.duration_sec.count() << ", " << 
                    new_session.system_start_time.time_since_epoch().count()  << ", " << 
                    new_session.system_end_time.time_since_epoch().count()    << ", " << 
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

        // NOTE(damian): 
        // Creating a tree to now have an invalid pointer error. 
        // Since tree is constructed of pointer to processes, there might be a time when there is a pointer to an already invalid data (Process).
        // It is not an issue if we create a tree right after updates every time in main. 
        // But sine there is a client getting data from us in paralel, there might be a slight edje case that we cant really test for.
        // I found it manually. 
        // At first didnt really like putting it here, but creating a tree is just a couple of loops, so should be fine.
        // If there is really an need to move it somewhere outtside of state_update, then just add some more boolean checks and ifs to guaranty pointer valisity.
        // (Make sure tree updates with the state)!.

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
             ++p_to_active) {
            if (p_to_active->data.exe_name == "Telegram.exe") {
                int x = 2;
            }
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
                if (tracked.compare(current)) { // NOTE(damian): not comaring as tracked, since then it would return Error for processes like chrome and vs code.
                    return Error(Error_type::tracked_and_current_process_vectors_share_data);
                }
            }
        }

        // Since list of tracked changed, changing the file that stores path for tracked processes
        std::error_code err_code_1;
        bool data_exists = fs::exists(G_state::file_path_with_tracked_processes, err_code_1);

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

        std::ofstream file(G_state::file_path_with_tracked_processes, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            return Error(Error_type::file_with_tracked_processes_doesnt_exist);
        }
        file << j_tracked.dump(4);
        file.close();

        // Creating a csv file with Session history for the process
        string process_path_copy = new_process.data.exe_path;
        for (char& ch : process_path_copy) {
            if (ch == '\\') ch = '-';
            if (ch == ':')  ch = '~';
        }

        std::filesystem::path csv_file_path;
        //csv_file_path.append(string(G_state::sessions_dir) + '\\' + process_path_copy); // NOTE(damian): why this notation and not just some nicely named function.
        csv_file_path.append(G_state::sessions_dir_path);
        csv_file_path.append(process_path_copy);
        std::cout << "Path: " << csv_file_path << std::endl;

        std::error_code err_code_2;
        bool exists = std::filesystem::exists(csv_file_path, err_code_2);

        if (err_code_2) {
            return Error(Error_type::filesystem_error);
        }

        if (!exists) { // Create the file
            std::ofstream new_file(csv_file_path);
            new_file << "";
            new_file.close();

            std::cout << "Create file for tracked process: " << csv_file_path << std::endl;
        }
        
        return Error(Error_type::ok);
    }

    G_state::Error remove_process_from_track(string* path) {
        bool is_tracked = false;
        auto p_to_tracked = G_state::tracked_processes.begin();
        for (;
             p_to_tracked != G_state::tracked_processes.end();
             ++p_to_tracked) {
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
        bool data_exists = fs::exists(G_state::file_path_with_tracked_processes, err_code_1);

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

        std::ofstream file(G_state::file_path_with_tracked_processes, std::ios::out | std::ios::trunc);
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
