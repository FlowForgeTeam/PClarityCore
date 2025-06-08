// TODO(damian): note why these are here for.
#include <iostream>
#include <fstream>
#include <tuple>
#include "Functions_win32.h"
#include "Functions_file_system.h"
#include "Global_state.h"

namespace G_state {
    const char* data_file_name = "data.json";
    vector<Process_data> currently_active_processes;
    vector<Process_data> tracked_processes;

	void set_up_on_startup() {
        // TODO: dont like all this error handleing here
        std::error_code err_code;

        bool data_exists = fs::exists(G_state::data_file_name, err_code);
        if (err_code) {
            // TODO(damian): handle better.
            std::cout << "Error on startup, the startup file was not found." << std::endl;
            assert(false);
        }

        if (data_exists) { // Read it, store it
            std::string text;
            read_file(G_state::data_file_name, &text);

            json data_as_json = json::parse(text);

            if (data_as_json.contains("processes_to_track")) {
                vector<json> processes_to_track = data_as_json["processes_to_track"];
                for(json& process_json : processes_to_track) {
                    // TODO(damian): this has to change, 
                    //               this is put here, to be re initialised inside the convert_from_json
                    Process_data process_data("fake_path"); 
                    convert_from_json(&process_json, &process_data);

                    G_state::tracked_processes.push_back(process_data);
                }
            }
            else {
                std::cout << "Error on startup, invalid data json." << std::endl;
                assert(false);
            }
        
        }
        else { // Create and store
            std::ofstream new_file(G_state::data_file_name);
            new_file << "{ \"processes_to_track\": [] }" << std::endl;
            new_file.close();
        }

    }

    static const int process_ids_buffer_len  = 512; // NOTE(damian): tested this with buffer size of 20, seems to be working.
    static const int process_path_buffer_len = 512; // NOTE(damian): tested this with buffer size of 20, seems to be working.
	void update_state() {
        // Getting active processes. The original buffer might be too small, so might need to allocate dyn.
        DWORD  stack_process_ids_buffer[process_ids_buffer_len];                            
        pair<DWORD*, int> pointer_len_pair = helper_get_all_active_processe_ids(stack_process_ids_buffer, process_ids_buffer_len);

        DWORD* process_ids_buffer = nullptr;
        bool heap_used_for_ids    = false;
        if (pointer_len_pair.first == nullptr) { 
            process_ids_buffer = stack_process_ids_buffer;
            heap_used_for_ids  = false;
        }
        else {
            process_ids_buffer = pointer_len_pair.first;
            heap_used_for_ids  = true;
        }
        int bytes_returned = pointer_len_pair.second;
        
        // Getting paths for processes, comparing to the once we are tracking, update respectively.
        int ids_returned = bytes_returned / sizeof(DWORD);
        for (int i = 0; i < ids_returned; ++i) {
            
            WCHAR stack_process_path_buffer[process_path_buffer_len];
            std::tuple<WCHAR*, int, Win32_error> triple = helper_get_process_path(process_ids_buffer[i], 
                                                                                  stack_process_path_buffer, 
                                                                                  process_path_buffer_len);  
            if (std::get<2>(triple) == Win32_error::win32_OpenProcess_failed) continue;
            if (std::get<2>(triple) == Win32_error::win32_EnumProcessModules_failed) continue;
            if (std::get<2>(triple) != Win32_error::ok) {
                std::cout << "Unhandled err_code." << std::endl;
                assert(false);
            }

            // TODO(damian): this is here now for clarity, move somewhere else.
            for (Process_data& tracked : G_state::tracked_processes) {
                for (Process_data& current : G_state::currently_active_processes) {
                    if (tracked == current) {
                        assert(false);
                    }
                }
            }

            WCHAR* process_path_buffer = nullptr;
            bool heap_used_for_name    = false;
            if (pointer_len_pair.first == nullptr) { 
                process_path_buffer = stack_process_path_buffer;
                heap_used_for_name  = false;
            }
            else {
                process_path_buffer = std::get<0>(triple);
                heap_used_for_name  = true;
            }
            int process_path_str_len = std::get<1>(triple);

            string buffer_as_string = wchar_to_utf8(process_path_buffer);

            // if (buffer_as_string == "C:\\Users\\Admin\\AppData\\Roaming\\Telegram Desktop\\Telegram.exe") {
            //     int x = 2;
            // }

            // Checking is this process is tracked, it yes, updating acordingly.
            bool is_tracked = false;
            for (Process_data& process_data : G_state::tracked_processes) {
                if (   process_data.path == buffer_as_string) {
                    // TODO(damian): since process_data.was_updated exists only to then know what to update as inactive,
                    //               we dont need it here as false. But it has to be false, 
                    //               if we reseted the data properly at the end of the state update. 
                    //                   ASSERT THIS. 

                    process_data.update_active();
                    is_tracked = true;
                    
                    // break; // This is commented, since we will miis some exes like chromo, since we only use path to identify.
                }
            }

            // It the process is not tracked, updating it acordingly.
            bool was_active_before = false;
            if (!is_tracked) {
                for (Process_data& process_data : G_state::currently_active_processes) {
                    if (process_data.path == buffer_as_string) {
                        // TODO(damian): since process_data.was_updated exists only to then know what to update as inactive,
                        //               we dont need it here as false. But it has to be false, 
                        //               if we reseted the data properly at the end of the state update. 
                        //                  ASSERT THIS. 

                        process_data.update_active();
                        was_active_before = true;
                        
                        // break; // This is commented, since we will miis some exes like chromo, since we only use path to identify.
                    }
                }

            }

            // If the process is not tracked, nor was it active during the last update
            if (!was_active_before && !is_tracked) {
                Process_data new_process(buffer_as_string.c_str());
                new_process.update_active();
                G_state::currently_active_processes.push_back(new_process);
            }
            
            // TODO(damian): think about having this as a reusable heap buffer, dyn allocation multiple times should be avoided.
            if (heap_used_for_name) free(process_path_buffer);
            
        }

        // Removing the processes that are not tracked and stopped being active
        for (auto it  = G_state::currently_active_processes.begin();
                  it != G_state::currently_active_processes.end()  ;)
        {
            if (!it->was_updated) {
                it = G_state::currently_active_processes.erase(it); // NOTE(damian): this doesnt invalidate iterator pointer.
            }
            else {
                ++it;
            }

        }

        // Updating inactive tracked processes
        for (Process_data& process_data : G_state::tracked_processes) {
            if (!process_data.was_updated)
                process_data.update_inactive();
        }

        // Reseting the was_updated bool for future updates.
        for (Process_data& process_data : G_state::currently_active_processes) {
            process_data.was_updated = false;
        }
        for (Process_data& process_data : G_state::tracked_processes) {
            process_data.was_updated = false;
        }

        if (heap_used_for_ids) free(process_ids_buffer);

    }

    G_state::Error add_process_to_track(string* path) {
        Process_data new_process(path);
        
        bool already_tracking = false;
        for (Process_data& process : G_state::tracked_processes) {
            if (process == new_process)
                already_tracking = true;
        }

        // see if it is inside the active once at this point, if it is, move it inside the tracke once.

        // maybe add some asserts in here

        // NOTE(damian): This should not be happening probably. 
        //               It might be ok for the cmd application, since in cmd user provides path himself.
        //               But in the UI appliocation, ui should never give the ability to user to work with invalid data.
        //               Ability to choose a process to track twice is invalid data.
        if (already_tracking)
            return G_state::Error::trying_to_track_the_same_process_more_than_once;    

        // Checking if it is already being tracked inside the vector active once
        auto p_to_active  = G_state::currently_active_processes.begin();
        bool found_active = false;
        for (;
             p_to_active != G_state::currently_active_processes.end(); 
             ++p_to_active)
        {
            if (*p_to_active == new_process)  {
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

        return G_state::Error::ok;
    }

    G_state::Error remove_process_from_track(string* path) {
        Process_data new_process(path);

        bool is_tracked   = false;
        auto p_to_tracked = G_state::tracked_processes.begin(); 
        for (;
             p_to_tracked != G_state::tracked_processes.end();
             ++p_to_tracked) 
        {
            if (*p_to_tracked == new_process) {
                is_tracked = true;
                break;
            }
        }

        if (!is_tracked) return G_state::Error::trying_to_untrack_a_non_tracked_process;

        G_state::tracked_processes.erase(p_to_tracked);

        return G_state::Error::ok;
    }

    // G_state::Error add_process_to_track(string* path) {
    //     Process_data new_process(*path);
        
    //     bool already_tracking = false;
    //     for (Process_data& process : G_state::tracked_processes) {
    //         if (process == new_process)
    //             already_tracking = true;
    //     }

    //     // see if it is inside the active once at this point, if it is, move it inside the tracke once.

    //     // maybe add some asserts in here

    //     // NOTE(damian): This should not be happening probably. 
    //     //               It might be ok for the cmd application, since in cmd user provides path himself.
    //     //               But in the UI appliocation, ui should never give the ability to user to work with invalid data.
    //     //               Ability to choose a process to track twice is invalid data.
    //     if (already_tracking)
    //         return G_state::Error::trying_to_track_the_same_process_more_than_once;    

    //     // Checking if it is already being tracked inside the vector active once
    //     auto p_to_active  = G_state::currently_active_processes.begin();
    //     bool found_active = false;
    //     for (;
    //          p_to_active != G_state::currently_active_processes.end(); 
    //          ++p_to_active)
    //     {
    //         if (*p_to_active == new_process)  {
    //             found_active = true;
    //             break;
    //         }
    //     }

    //     // Moving from cur_active to tracked
    //     if (found_active) {
    //         p_to_active->is_tracked = true;
    //         G_state::tracked_processes.push_back(std::move(*p_to_active));
    //         G_state::currently_active_processes.erase(p_to_active);
    //     }
    //     else { // Just adding to tracked
    //         new_process.is_tracked = true;
    //         G_state::tracked_processes.push_back(new_process);
    //     }

    //     return G_state::Error::ok;
    // }

    // G_state::Error remove_process_from_track(string* path) {
    //     Process_data new_process(*path);

    //     bool is_tracked   = false;
    //     auto p_to_tracked = G_state::tracked_processes.begin(); 
    //     for (;
    //          p_to_tracked != G_state::tracked_processes.end();
    //          ++p_to_tracked) 
    //     {
    //         if (*p_to_tracked == new_process) {
    //             is_tracked = true;
    //             break;
    //         }
    //     }

    //     if (!is_tracked) return G_state::Error::trying_to_untrack_a_non_tracked_process;

    //     G_state::tracked_processes.erase(p_to_tracked);

    //     return G_state::Error::ok;
    // }

    


}
