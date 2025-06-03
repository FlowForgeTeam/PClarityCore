// TODO(damian): note why these are here for.
#include <iostream>
#include <fstream>
#include "Functions_win32.h"
#include "Functions_file_system.h"
#include "Global_state.h"

namespace G_state {
    const char* data_file_name = "data.json";
    vector<Process_data> process_data_vec;

	void set_up_on_startup() {
        // TODO: dont like all this error handleing here
        std::error_code err_code;

        bool data_exists = fs::exists(G_state::data_file_name, err_code);
        if (err_code) {
            // TODO: handle os error.
        }

        if (data_exists) { // Read it, store it
            std::string text;
            read_file(G_state::data_file_name, &text);

            json data_as_json = json::parse(text);

            if (data_as_json.contains("processes_to_track")) {
                vector<json> processes_to_track = data_as_json["processes_to_track"];
                for(json& process_json : processes_to_track) {
                    Process_data process_data(wstring(L"fake_name"), 123); // TODO(damian): This fale inti sucks here 
                    convert_from_json(&process_json, &process_data);

                    G_state::process_data_vec.push_back(process_data);
                }
            }
            else {
                // TODO: invalid json, report.
            }

        
            // TODO: init the map here
        }
        else { // Create and store
            std::ofstream new_file(G_state::data_file_name);
            new_file << "{ \"processes_to_track\": [] }" << std::endl;
            new_file.close();
        }

    }

    // This doesnt work.dwwwadaw
	void update_state() {
        // TODO(damian): a lot of these nested ifs and loops will probably 
        //               never be used elese where, but we might want to split them into
        //               separate funcs, just for clarity reasons.

        // TODO(damian): handle the overflow of this and request again.
        DWORD process_ids_buffer[512];                            
        std::pair<int, Win32_error> result = get_all_active_processe_ids(process_ids_buffer, 512);
        if (result.second == Win32_error::win32_EnumProcesses_failed) {
            // TODO:
        }
        else if (result.second == Win32_error::win32_EnumProcess_buffer_too_small) {
            // TODO:

        }
        else if (result.second != Win32_error::ok) {
            std::cout << "Unhandled exception was caught." << std::endl;
            assert(false);
        }

        int bytes_returned = result.first;

        for (int i = 0; i < bytes_returned / sizeof(DWORD); ++i) {
            WCHAR process_name_buffer[512];
            Win32_error err_code = get_process_name(process_ids_buffer[i], process_name_buffer, 512);
            if (err_code == Win32_error::win32_OpenProcess_failed) {
                // TODO:
            }
            else if (err_code == Win32_error::win32_EnumProcessModules_failed) {
                // TODO:
            }
            else if (err_code == Win32_error::win32_GetModuleBaseName_failed) {
                // TODO:
            }
            else if (err_code == Win32_error::win32_GetModuleBaseName_buffer_too_small) {
                // TODO:
            }
            else if (err_code != Win32_error::ok) {
                std::cout << "Unhandled exception was caught." << std::endl;
                assert(false);
            }

            auto temp = wchar_to_utf8(process_name_buffer);
            if (temp == string("Telegram.exe")) {
                int x = 2;
            }

            for (Process_data& process_data : G_state::process_data_vec) {
                auto test_1 = process_data.name;
                auto test_2 = wchar_to_utf8(process_name_buffer);
                if (process_data.name == wchar_to_utf8(process_name_buffer) && process_data.was_updated == false) {
                    process_data.update_active();
                }
                int x = 2;
            }
            
        }

        for (Process_data& process_data : G_state::process_data_vec) {
            if (process_data.was_updated == false)
                process_data.update_inactive();
        }

        // Reseting the was_updated bool
        for (Process_data& process_data : G_state::process_data_vec) {
            process_data.was_updated = false;
        }

        //============================================================================================



    }

   
}