#include "Functions_win32.h"
#include <filesystem>
#include <fstream>      // NOTE(damian): this is use later for file initial data in setup.

namespace fs = std::filesystem;

// TODO: maybe havethese weid win32 typedefs briefly documented somewhere here.
// TODO: maybe return the error code as return and bytes_returned as an optinal parameter list pointer.
int get_all_active_processe_ids(DWORD* process_ids_arr, size_t arr_len) {
    DWORD number_of_bytes_returned;

    // TODO: check once again the return type for this.
    int err_code = EnumProcesses(process_ids_arr, (DWORD)(arr_len * sizeof(DWORD)), &number_of_bytes_returned);
    if (err_code == 0) {
        return -1; // EnumProcesses failed
    }

    if (number_of_bytes_returned == arr_len * sizeof(DWORD)) {
        return -1; // Passed in array was to small to capture all the processes
    }

    return number_of_bytes_returned;     // Success
}

// TODO: need better error reports here, but no Thorws. Maybe Result wouldnt do it as well, have to see syntax for it in c++.
int get_process_name(DWORD process_id, WCHAR* name_buffer, size_t name_buffer_len) {
    // TODO: see if using ACCESS_ALL is better here.
    HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, // These specify the access rights
                                        FALSE,                                       // This is for process creation, we dont need it, so FALSE
                                        process_id);                                 // Process id to get a handle of
    
    if (process_handle != NULL) {
        HMODULE module_handle;              // NOTE(damian): this is the same as HINSTANCE. This is a legacy type, back from 16-bit windows. 
        DWORD   number_of_bytes_returned;

        // Getting module for the process_id
        if (EnumProcessModules(process_handle, &module_handle, sizeof(module_handle), &number_of_bytes_returned) != 0) {        
            // Getting the name for the module_handle
            DWORD name_buffer_new_len = 
                GetModuleBaseNameW(process_handle, module_handle, name_buffer, name_buffer_len / sizeof(WCHAR));
            if (name_buffer_new_len == 0) return 1;
            if (name_buffer_new_len == name_buffer_len) return 2;

        }
        else return 3;

        CloseHandle(process_handle);  // Have to close it. Shit goes south If not closed --> // TODO: see why
        return 0;
    }

    return 4;
}

// TODO: need better error reports here, but no Thorws. Maybe Result wouldnt do it as well, have to see syntax for it in c++.
int get_process_path(DWORD process_id, WCHAR* path_buffer, size_t path_buffer_len) {
    HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, // These specify the access rights
                                        FALSE,                                       // This is for process creation, we dont need it, so FALSE
                                        process_id);                                // Process id to get a handle of
    
    if (process_handle != NULL) {
        // NOTE(damian): This does not need to be closed.
        // NOTE(damian): this is the same as HINSTANCE. This is a legacy type, back from 16-bit windows.
        HMODULE module_handle;               
        DWORD   number_of_bytes_returned;

        // Getting module for the process_id
        if (EnumProcessModules(process_handle, &module_handle, sizeof(module_handle), &number_of_bytes_returned) != 0) {        
            // Getting the path of the module handle.
            DWORD path_buffer_new_len = 
                GetModuleFileNameExW(process_handle, module_handle, path_buffer, path_buffer_len / sizeof(WCHAR));
            if (path_buffer_new_len == 0) return 1;
            if (path_buffer_new_len == path_buffer_len) return 2;

        }
        else return 3;

        CloseHandle(process_handle);  // Have to close it. Shit goes south If not closed --> // TODO: see why
        return 0;
    }

    return 4;
}


// bool see_if_process_is_active(DWORD* process_ids_arr, size_t arr_len, const WCHAR* process_name) {
//     WCHAR process_name_buffer[256] = TEXT("<unknown>");

//     for (int i = 0; i < arr_len; ++i) {
//         HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, // These specify the access rights
//             FALSE,                                       // This is for process creation, we dont need it, so FALSE
//             process_ids_arr[i]);                         // Process id to get a handle of

//         if (process_handle != NULL) {
//             HMODULE module_handle;              // NOTE(damian): this is the same as HINSTANCE. This is a legacy type, back from 16-bit windows. 
//             DWORD   number_of_bytes_returned;

//             // Getting module for the process_id
//             if (EnumProcessModules(process_handle, &module_handle, sizeof(module_handle), &number_of_bytes_returned) != 0) {
//                 // Getting the name for the module_handle
//                 GetModuleBaseName(process_handle, module_handle, process_name_buffer, sizeof(process_name_buffer) / sizeof(WCHAR));

//                 // Comparing the names using wcscmp, since the  strings are using w_chars
//                 if (wcscmp(process_name_buffer, process_name) == 0) {
//                     CloseHandle(process_handle);  // Have to close it. Shit goes south If not closed --> // TODO: see why
//                     return true;
//                 }

//             }
//             CloseHandle(process_handle);  // Have to close it. Shit goes south If not closed --> // TODO: see why

//         }
//     }

//     return false;
// }


// // TODO: maybe use assert or some else insted of printf. 
// // TODO: think about logging this into some sort of text file.
// // TODO: 

// bool is_process_active_by_name(const WCHAR* process_name) {
//     DWORD array_of_process_ids[1024];

//     int err_code = get_all_active_processe_ids(array_of_process_ids, 1024);
//     if (err_code == 1) {
//         printf("Was not able to retrive active processes, OS failed. \n");
//         exit(1);
//     }
//     else if (err_code == 2) { // TODO: this is bad, allocate more.
//         printf("Was not able to capture all processes, the passed in arr was to small. \n");
//         exit(1);
//     }

//     bool is_active = see_if_process_is_active(array_of_process_ids, 1024, process_name);

//     return is_active;
// }
