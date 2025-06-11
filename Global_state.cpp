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

	G_state::Error set_up_on_startup() {
        bool data_exists = fs::exists(G_state::data_file_name);
        // TODO(damian): uncommnet it. Leaving it commented till release.
        // if (!data_exists) {
        //     return G_state::Error::data_file_doesnt_exist;
        // }

        if (data_exists) { // Read it, store it
            std::string text;
            read_file(G_state::data_file_name, &text);

            json data_as_json; 
            try {
                data_as_json = json::parse(text);
            }
            catch(...) {
                return G_state::Error::json_parsing_failed;
            }

            if (data_as_json.contains("processes_to_track")) {
                vector<json> processes_to_track = data_as_json["processes_to_track"];
                for(json& process_json : processes_to_track) {

                    // TODO(damian): this has to change, 
                    //               this is put here, to be re initialised inside the convert_from_json
                    Process_data process_data(Win32_process_data{0});
                    convert_from_json(&process_json, &process_data);
                    process_data.is_tracked = true;

                    G_state::tracked_processes.push_back(process_data);
                }
            }
            else {
                return G_state::Error::json_invalid_strcture;
            }
        
        }
        else { // Create and store
            std::ofstream new_file(G_state::data_file_name);
            new_file << "{ \"processes_to_track\": [] }" << std::endl;
            new_file.close();
        }

        return G_state::Error::ok;
    }

    G_state::Error G_state::update_state() {
        pair<vector<Win32_process_data>, Win32_error> result = win32_get_process_data();
        if (result.second != Win32_error::ok) {
            // TODO(damian): handle better.
            assert(false);
        }

        // NOTE(damian): for code clarity.
        for (Process_data& data : G_state::tracked_processes) {
            if (data.was_updated) assert(false);
        }
        for (Process_data& data : G_state::currently_active_processes) {
            if (data.was_updated) assert(false);
        }

        // Updating the state
        for (Win32_process_data& win32_data : result.first) {
            
            // NOTE(damian): for manual hand testing )).
            if (win32_data.exe_name == "Telegram.exe") {
                int x = 2;
            }

            bool is_tracked = false;
            for (Process_data& g_state_data : G_state::tracked_processes) {
                if (g_state_data == win32_data) {
                    g_state_data.update_active();
                    is_tracked = true;

                    // TODO(damian): maybe assert to make sure that there is no other exact process.
                }
            }

            bool was_active_before = false;
            if (!is_tracked) {
                for (Process_data& g_state_data : G_state::currently_active_processes) {
                    if (g_state_data == win32_data) {
                        g_state_data.update_active();
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
        for (auto it  = G_state::currently_active_processes.begin();
                  it != G_state::currently_active_processes.end()  ;)
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

        return G_state::Error::ok;
    }

    G_state::Error add_process_to_track(string* path) {
        Win32_process_data win32_data = { 0 };

        // TODO(damian): this is temporaty.
        win32_data.exe_path = *path;

        Process_data new_process(win32_data);
        
        bool already_tracking = false;
        for (Process_data& process : G_state::tracked_processes) {
            if (process == new_process)
                already_tracking = true;
        }

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

        // TODO(damian): this is here now for clarity, move somewhere else.
        for (Process_data& tracked : G_state::tracked_processes) {
            for (Process_data& current : G_state::currently_active_processes) {
                if (tracked == current) {
                    return G_state::Error::tracked_and_current_process_vectors_share_data;
                }
            }
        }

        return G_state::Error::ok;
    }

    G_state::Error remove_process_from_track(string* path) {
        bool is_tracked   = false;
        auto p_to_tracked = G_state::tracked_processes.begin(); 
        for (;
            p_to_tracked != G_state::tracked_processes.end();
            ++p_to_tracked) 
        {
            if (p_to_tracked->data.exe_path == *path) {
                is_tracked = true;
                break;
            }
        }

        if (!is_tracked) return G_state::Error::trying_to_untrack_a_non_tracked_process;

        G_state::tracked_processes.erase(p_to_tracked);

        return G_state::Error::ok;
    }

}

























