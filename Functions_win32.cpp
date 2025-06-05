#include "Functions_win32.h"
#include <filesystem>
#include <fstream>      

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




























