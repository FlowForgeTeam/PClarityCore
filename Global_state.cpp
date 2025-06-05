// TODO(damian): note why these are here for.
#include <iostream>
#include <fstream>
#include <tuple>
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
                    // TODO(damian): this has to chawenge, 
                    //               this is put here, to be re initialised inside the convert_from_json
                    Process_data process_data(string("fake_path"), 123); 
                    convert_from_json(&process_json, &process_data);

                    G_state::process_data_vec.push_back(process_data);
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
    static pair<DWORD*, int> helper_get_all_active_processe_ids(DWORD* process_ids_buffer_on_stack, int process_ids_buffer_on_stack_len);
    static std::tuple<WCHAR*, int, Win32_error> helper_get_process_path(DWORD process_id, WCHAR* stack_process_path_buffer, int stack_process_path_buffer_len);
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
        
        // Getting names for processes, comparing to the once we are tracking, update respectively.
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

            auto temp = wchar_to_utf8(process_path_buffer);

            for (Process_data& process_data : G_state::process_data_vec) {
                if (  
                    process_data.path == wchar_to_utf8(process_path_buffer)
                    && process_data.was_updated == false) {
                    process_data.update_active();
                }
            }

            // TODO(damian): think about having this a reusable heap buffer, dyn allocation multiple times should be avoided
            if (heap_used_for_name) free(process_path_buffer);
            
        }

        for (Process_data& process_data : G_state::process_data_vec) {
            if (process_data.was_updated == false)
                process_data.update_inactive();
        }

        // Reseting the was_updated bool
        for (Process_data& process_data : G_state::process_data_vec) {
            process_data.was_updated = false;
        }

        if (heap_used_for_ids) free(process_ids_buffer);


    }

	void add_process_to_track(Process_data new_process) {
        bool already_tracking = false;
        for (Process_data& process : G_state::process_data_vec) {
            if (process == new_process)
                already_tracking = true;
        }

        if (already_tracking) {
            // NOTE(damian): This should not be happening probably. 
            //               It might be ok for the cmd application, since in cmd user provides path himself.
            //               But in the UI appliocation, ui should never give the ability to user to work with invalid data.
            //               Ability to choose a process to track twice is invalid data.
            assert(false); // TODO(damian): handle better. 
        }

        G_state::process_data_vec.push_back(new_process);

    }


    // NOTE(damian): this gets all the active process, deals with static buffer overflow. 
    //               if static buffer is too small, uses a bigger dynamic one. 
    //               if a dynamic one was used, pointer to it, allong with the arr length will be returned, 
    //                  else returs nullptr.
    //               DONT FORGET TO DELETE THE DYNAMIC MEMORY LATER
    static pair<DWORD*, int> helper_get_all_active_processe_ids(DWORD* process_ids_buffer_on_stack, int process_ids_buffer_on_stack_len) {
        DWORD* process_ids_buffer_on_heap     = nullptr;
        int    process_ids_buffer_on_heap_len = process_ids_buffer_on_stack_len * 2;
        int    bytes_returned                 = -1;  

        while(true) {
            std::pair<int, Win32_error> result;
            if (process_ids_buffer_on_heap == nullptr)
                result = get_all_active_processe_ids(process_ids_buffer_on_stack, process_ids_buffer_on_stack_len);
            else 
                result = get_all_active_processe_ids(process_ids_buffer_on_heap, process_ids_buffer_on_heap_len);

            if (result.second == Win32_error::win32_EnumProcesses_failed) {
                std::cout << "Error when getting active windows processes." << std::endl;
                assert(false);
            }
            else if (result.second == Win32_error::win32_EnumProcess_buffer_too_small) {
                if (process_ids_buffer_on_heap == nullptr) {
                    process_ids_buffer_on_heap = (DWORD*) malloc(sizeof(DWORD) * process_ids_buffer_on_heap_len);
                }
                else {
                    process_ids_buffer_on_heap_len *= 2;
                    int bytes_needed                = sizeof(DWORD) * process_ids_buffer_on_heap_len;
                    process_ids_buffer_on_heap = (DWORD*) realloc(process_ids_buffer_on_heap, bytes_needed);
                }

                if (process_ids_buffer_on_heap == NULL) {
                    std::cout << "Not enought memory to allocate a buffer." << std::endl;
                    assert(false);
                }

            }
            else if (result.second == Win32_error::ok) {
                bytes_returned = result.first;
                break;
            }
            else {
                std::cout << "Unhandeled error." << std::endl;
                    assert(false);
            }
        }
        assert(bytes_returned != -1);

        return pair(process_ids_buffer_on_heap, bytes_returned);
    }

    // NOTE(damian): this gets the name for the provided process id, deals with static buffer overflow. 
    //               if static buffer is too small, uses a bigger dynamic one. 
    //               if a dynamic one was used, pointer to it, allong with the name string length will be returned, 
    //                  else returs nullptr.
    //               Error is returned as the thirf element, this is done in order to skip the unaccesible processes.
    //               DONT FORGET TO DELETE THE DYNAMIC MEMORY LATER
    static std::tuple<WCHAR*, int, Win32_error> helper_get_process_path(DWORD process_id, WCHAR* stack_process_path_buffer, int stack_process_path_buffer_len) {
        WCHAR* process_path_buffer_on_heap     = nullptr;
        int    process_path_buffer_on_heap_len = stack_process_path_buffer_len * 2;
        int    process_path_str_len            = -1;  

        while(true) {
            std::pair<int, Win32_error> result;
            if (process_path_buffer_on_heap == nullptr)
                result = get_process_path(process_id, stack_process_path_buffer, stack_process_path_buffer_len);
            else 
                result = get_process_path(process_id, process_path_buffer_on_heap, process_path_buffer_on_heap_len);
            
            if (result.second == Win32_error::win32_OpenProcess_failed) {
                /*std::cout << "Error, was not able to open a process." << std::endl;
                assert(false);*/
                // NOTE(damian): Some procecces are not accasible due to security and some other reasons, 
                //               so this failing is probably ok.
                return std::tuple(nullptr, -1, Win32_error::win32_OpenProcess_failed);
            }
            else if (result.second == Win32_error::win32_EnumProcessModules_failed) {
                /*std::cout << "Error, was not able to enum process modules." << std::endl;
                assert(false);*/
                // NOTE(damian): Not sure if this failing is ok.
                return std::tuple(nullptr, -1, Win32_error::win32_EnumProcessModules_failed);
            }
            else if (result.second == Win32_error::win32_GetModuleFileNameExW_buffer_too_small) {
                if (process_path_buffer_on_heap == nullptr) {
                    process_path_buffer_on_heap = (WCHAR*) malloc(sizeof(WCHAR) * process_path_buffer_on_heap_len);
                }
                else {
                    process_path_buffer_on_heap_len *= 2;
                    int bytes_needed                 = sizeof(WCHAR) * process_path_buffer_on_heap_len;
                    process_path_buffer_on_heap = (WCHAR*) realloc(process_path_buffer_on_heap, bytes_needed);
                }

                if (process_path_buffer_on_heap == NULL) {
                    std::cout << "Not enought memory to allocate a buffer." << std::endl;
                    assert(false);
                }
            }
            else if (result.second == Win32_error::ok) {
                process_path_str_len = result.first;
                break;
            }
            else {
                std::cout << "Unhandeled error." << std::endl;
                    assert(false);
            }
        }
        assert(process_path_str_len != -1);

        return std::tuple(process_path_buffer_on_heap, process_path_str_len, Win32_error::ok);

    }
    

}
