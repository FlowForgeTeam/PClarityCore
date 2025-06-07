#include "Functions_win32.h"
#include <filesystem>
#include <fstream>      
#include "assert.h"
#include <iostream>

namespace fs = std::filesystem;

// TODO: maybe have these weird win32 typedefs briefly documented somewhere here.


pair<int, Win32_error> get_all_active_processe_ids(DWORD* process_ids_arr, size_t arr_len) {
    DWORD number_of_bytes_returned;
    int err_code = EnumProcesses(process_ids_arr, 
                                 (DWORD)(arr_len * sizeof(DWORD)), 
                                 &number_of_bytes_returned);
    
    if (err_code == 0) {
        return pair(0, Win32_error::win32_EnumProcesses_failed);
    }

    if (number_of_bytes_returned == arr_len * sizeof(DWORD)) {
        return pair(0, Win32_error::win32_EnumProcess_buffer_too_small);
    }

    return pair(number_of_bytes_returned, Win32_error::ok);     // Success
}

pair<int, Win32_error> get_process_path(DWORD process_id, WCHAR* path_buffer, size_t path_buffer_len) {
    // TODO: see if using ACCESS_ALL is better here.
    HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, // These specify the access rights
                                        FALSE,                                       // This is for process creation, we dont need it, so FALSE
                                        process_id);                                 // Process id to get a handle of
    if (process_handle == NULL) return pair(0, Win32_error::win32_OpenProcess_failed);
    
    HMODULE module_handle;              // NOTE(damian): this is the same as HINSTANCE. This is a legacy type, back from 16-bit windows. 
    DWORD   number_of_bytes_returned;

    // NOTE(damian): doesnt null terminate. So if overflow, then the name for sure did not fit. No \0.
    int err_code = EnumProcessModules(process_handle, 
                                      &module_handle, 
                                      sizeof(module_handle), 
                                      &number_of_bytes_returned);
    if (err_code == 0) {
        return pair(0, Win32_error::win32_EnumProcessModules_failed);
    }
    DWORD path_buffer_new_len =  GetModuleFileNameExW(process_handle, 
                                                      module_handle, 
                                                      path_buffer, 
                                                      path_buffer_len / sizeof(WCHAR));
    if (path_buffer_new_len == 0) return pair(0, Win32_error::win32_GetModuleFileNameExW_failed);
    if (path_buffer_new_len == path_buffer_len) return pair(0, Win32_error::win32_GetModuleFileNameExW_buffer_too_small);

    CloseHandle(process_handle);

    return pair(path_buffer_new_len, Win32_error::ok);
}


// NOTE(damian): this gets all the active process, deals with static buffer overflow. 
//               if static buffer is too small, uses a bigger dynamic one. 
//               if a dynamic one was used, pointer to it, allong with the arr length will be returned, 
//                  else returs nullptr.
//               DONT FORGET TO DELETE THE DYNAMIC MEMORY LATER
pair<DWORD*, int> helper_get_all_active_processe_ids(DWORD* process_ids_buffer_on_stack, int process_ids_buffer_on_stack_len) {
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
std::tuple<WCHAR*, int, Win32_error> helper_get_process_path(DWORD process_id, WCHAR* stack_process_path_buffer, int stack_process_path_buffer_len) {
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
    

























