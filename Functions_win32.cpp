#include <fstream>      
#include <iostream>
#include <Windows.h>
#include <tlhelp32.h>
#include <cassert>
#include <nlohmann/json.hpp>

#include "Functions_win32.h"
// #pragma comment(lib, "Kernel32") // Linking the dll

// TODO(damian): if all these win32 function fail for the same reason then skipping this process is fine,
//               but if they dont, we would be better of getting some public info for the process, 
//               rather then getting none. (Just something to think about).

// NOTE(damian): this was claude generated, need to make sure its is correct. 
std::string wchar_to_utf8(const WCHAR* wstr) {
    if (!wstr) return "";

    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";

    std::string result(size - 1, 0); // size-1 to exclude null terminator
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], size, nullptr, nullptr);

    return result;
}

std::pair<vector<Win32_process_data>, Win32_error> win32_get_process_data() {
    HANDLE process_shapshot_handle;
    PROCESSENTRY32 pe32;

    // Take a snapshot of all processes in the system.
    process_shapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (process_shapshot_handle == INVALID_HANDLE_VALUE) {
        return pair(vector<Win32_process_data>(), Win32_error::CreateToolhelp32Snapshot_failed);
    }

    // Set the size of the structure before using it.
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(process_shapshot_handle, &pe32)) {
        CloseHandle(process_shapshot_handle);
        return pair(vector<Win32_process_data>(), Win32_error::Process32First_failed);
    }

    vector<Win32_process_data> process_data_vec;

    do {
        // Opening a handle, all the data for the process is kept until all the handles are released.
        HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                            FALSE,
                                            pe32.th32ProcessID);
        if (process_handle == NULL) {
            continue;
        }

        // 1. Snapshot data
        // 2. Full process path
        // 3. Priority
        // 4. Process times
        // 5. Affinity masks
        // 6. RAM

        // 1.
        Win32_process_data data = {0};
        data.pid             = pe32.th32ProcessID;
        data.ppid            = pe32.th32ParentProcessID;
        data.exe_name        = wchar_to_utf8(pe32.szExeFile);
        data.base_priority   = pe32.pcPriClassBase;
        data.started_threads = pe32.cntThreads;

        if (data.exe_name == "Telegram.exe") {
            int x = 2;
        }

        // 2.
        const size_t stack_buffer_len = 256; // NOTE(damian): tested this with size of 5, woked well. 
        WCHAR stack_buffer[stack_buffer_len];
        tuple<WCHAR*, bool, DWORD, Win32_error> result = win32_get_path_for_process(process_handle, 
                                                                                    stack_buffer, 
                                                                                    stack_buffer_len);
        WCHAR* buffer         = std::get<0>(result);
        bool   is_buffer_heap = std::get<1>(result);
        DWORD  buffer_len     = std::get<2>(result);
        Win32_error err_code  = std::get<3>(result);
        
        if (err_code != Win32_error::ok) { continue; }

        data.exe_path = wchar_to_utf8(buffer);
        
        if(is_buffer_heap) { free(buffer); }

        // TODO(damian): when having a process snapshot, 
        //               need to figure out how we can tell if the process has ended to the pid is invlid now.
        // NOTE(damian): using GetExitCodeProcess is the answer. 

        // 3.
        DWORD priority = GetPriorityClass(process_handle);
        if (priority == 0) {
            CloseHandle(process_handle); 
            continue;
        }
        data.priority_class = priority;

        // 4.
        FILETIME process_creation_f_time;
        FILETIME process_last_exit_f_time;    // Counts of 100-nanosecond time units
        FILETIME process_kernel_f_time;
        FILETIME process_user_f_time;
        if (GetProcessTimes(process_handle, 
                            &process_creation_f_time, 
                            &process_last_exit_f_time, 
                            &process_kernel_f_time, 
                            &process_user_f_time) == 0
        ) {
            CloseHandle(process_handle);
            continue;
        }

        ULARGE_INTEGER process_creation_time;
        process_creation_time.LowPart  = process_creation_f_time.dwLowDateTime;
        process_creation_time.HighPart = process_creation_f_time.dwHighDateTime;
        data.process_creation_time = process_creation_time.QuadPart;

        ULARGE_INTEGER process_exit_time;
        process_exit_time.LowPart  = process_last_exit_f_time.dwLowDateTime;
        process_exit_time.HighPart = process_last_exit_f_time.dwHighDateTime;
        data.process_exit_time = process_exit_time.QuadPart;

        ULARGE_INTEGER process_kernel_time;
        process_kernel_time.LowPart  = process_kernel_f_time.dwLowDateTime;
        process_kernel_time.HighPart = process_kernel_f_time.dwHighDateTime;
        data.process_kernel_time = process_kernel_time.QuadPart;

        ULARGE_INTEGER process_user_time;
        process_user_time.LowPart  = process_user_f_time.dwLowDateTime;
        process_user_time.HighPart = process_user_f_time.dwHighDateTime;
        data.process_user_time = process_user_time.QuadPart;

        // Getting system times
        FILETIME system_idle_f_time, system_kernel_f_time, system_user_f_time;
        if (GetSystemTimes(&system_idle_f_time, 
                           &system_kernel_f_time, 
                           &system_user_f_time) == 0
        ) {
            CloseHandle(process_handle);
            continue;
        }

        ULARGE_INTEGER system_idle_time;
        system_idle_time.LowPart  = system_idle_f_time.dwLowDateTime;
        system_idle_time.HighPart = system_idle_f_time.dwHighDateTime;
        data.system_idle_time = system_idle_time.QuadPart;

        ULARGE_INTEGER system_kernel_time;
        system_kernel_time.LowPart  = system_kernel_f_time.dwLowDateTime;
        system_kernel_time.HighPart = system_kernel_f_time.dwHighDateTime;
        data.system_kernel_time = system_kernel_time.QuadPart;

        ULARGE_INTEGER system_user_time;
        system_user_time.LowPart  = system_user_f_time.dwLowDateTime;
        system_user_time.HighPart = system_user_f_time.dwHighDateTime;
        data.system_user_time = system_user_time.QuadPart;

        // 5.
        SIZE_T process_affinity_mask;
        SIZE_T system_affinity_mask;
        if(GetProcessAffinityMask(process_handle, &process_affinity_mask, &system_affinity_mask) == 0) {
            CloseHandle(process_handle); 
            continue;
        }
        data.process_affinity = process_affinity_mask;
        data.system_affinity  = system_affinity_mask;

        // 6.
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(process_handle, &pmc, sizeof(pmc)) == 0) {
            CloseHandle(process_handle); 
            continue;
        }
        data.ram_usage = pmc.WorkingSetSize;

        // GPU
        // SOME OTHER STUFF (PROCESS LASSO TYPE SHIT)
        
        // NOTE(damian): the name for the process might also be retrived via QueryFullProcessImageNameA.
        //               currently we get it from the first module inside the process modules vector.
        //               based on the docs, the first module retrived by the Module32First function 
        //               always contais data for the process whos modules are retrived.

        // data.is_visible_app = win32_is_process_an_app(process_handle, &data);

        process_data_vec.push_back(data);

        CloseHandle(process_handle);

    } while (Process32Next(process_shapshot_handle, &pe32));

    CloseHandle(process_shapshot_handle);

    return pair(process_data_vec, Win32_error::ok);
}

tuple<WCHAR*, bool, DWORD, Win32_error> win32_get_path_for_process(HANDLE process_handle,
    WCHAR* stack_buffer,
    size_t stack_buffer_len)
{
    DWORD path_len = stack_buffer_len;
    BOOL  err      = QueryFullProcessImageNameW(process_handle, NULL, stack_buffer, &path_len); // Does null terminate.

    if (err != NULL) {
        return tuple(stack_buffer, false, path_len, Win32_error::ok);
    }
    else if (GetLastError() != 122) { // NOTE(damian): 122 is code for insufficient buffer size.
        return tuple(stack_buffer, false, path_len, Win32_error::QueryFullProcessImageNameW);
    }

    // Need to use heap
    size_t heap_buffer_len = stack_buffer_len * 2;
    WCHAR* heap_buffer     = nullptr;

    while(true) {
        if (heap_buffer == nullptr) {
            heap_buffer = (WCHAR*) malloc(sizeof(WCHAR) * heap_buffer_len);
        }
        else {
            heap_buffer_len *= 2;
            heap_buffer = (WCHAR*) realloc(heap_buffer, sizeof(WCHAR) * heap_buffer_len);
        }

        if (heap_buffer == NULL) {
            assert(false);
            // TODO(damian): handle better.
        }

        DWORD path_len = heap_buffer_len;
        BOOL  err      = QueryFullProcessImageNameW(process_handle, NULL, heap_buffer, &path_len);
        
        if (err != NULL) {
            //free(heap_buffer);
            return tuple(heap_buffer, true, path_len, Win32_error::ok);
        }
        else if (GetLastError() != 122) { // NOTE(damian): 122 is code for insufficient buffer size. Not it, so some other error.
            return tuple(stack_buffer, false, path_len, Win32_error::QueryFullProcessImageNameW);
        }

    }

}


static bool is_visible_app = false; 
static BOOL CALLBACK win32_is_process_an_app_callback(HWND window_handle, LPARAM lParam);
bool win32_is_process_an_app(HANDLE process_handle, Win32_process_data* data) {
    if (data->exe_name == "Telegram.exe") {
        int x = 2;
    }

    BOOL err_code = EnumWindows(win32_is_process_an_app_callback, data->pid);

    if (is_visible_app) {
        is_visible_app = false;
        return true;
    }

    return false;
}

BOOL CALLBACK win32_is_process_an_app_callback(HWND window_handle, LPARAM lParam) {
    // Check if there is an active widnow, if yes, return FALSE and set LPARAM to represent it.

    DWORD window_creator_pid;
    if (GetWindowThreadProcessId(window_handle, &window_creator_pid) == 0) {
        return TRUE; // Just skipping it
        // TODO(damian): dont know why this might fails, since we only get widnow handles by the win32 enum function.
    }
    
    // GetWindow failed, so keep enumerating by returning TRUE
    if (window_creator_pid == 0) return TRUE; 

    // GetWindow didnt fail, its creator process is the passed in pid (lParam) and it is visible --> 
    //      --> set the flag to true, return FALSE to stop iterating over windows
    if (window_creator_pid == (DWORD) lParam && IsWindowVisible(window_handle)) {
        is_visible_app = true;
        return FALSE;
    } 

    return TRUE;
}


// =============================================================================================


//void convert_to_json(Win32_process_data* win32_data, json* j) {
//    (*j)["pid"]              = win32_data->pid;
//    (*j)["started_threads"]  = win32_data->started_threads;
//    (*j)["ppid"]             = win32_data->ppid;
//    (*j)["base_priority"]    = win32_data->base_priority;
//    try {
//        (*j)["exe_name"]         = win32_data->exe_name;
//
//    }
//    catch (...) {
//        int x = 2;
//    }
//    (*j)["exe_path"]         = win32_data->exe_path;
//    (*j)["priority_class"]   = win32_data->priority_class;
//    (*j)["creation_time"]    = win32_data->creation_time;  
//    (*j)["exit_time"]        = win32_data->exit_time;          
//    (*j)["kernel_time"]      = win32_data->kernel_time;    
//    (*j)["user_time"]        = win32_data->user_time;     
//    (*j)["process_affinity"] = win32_data->process_affinity;
//    (*j)["system_affinity"]  = win32_data->system_affinity;
//    (*j)["ram_usage"]        = win32_data->ram_usage;
//    // (*j)["is_visible_app"]   = win32_data->is_visible_app;
//}

//bool convert_from_json(Win32_process_data* win32_data, json* j) {
//    try {
//        win32_data->pid              = (*j)["pid"];
//        win32_data->started_threads  = (*j)["started_threads"];
//        win32_data->ppid             = (*j)["ppid"];
//        win32_data->base_priority    = (*j)["base_priority"];
//        win32_data->exe_name         = (*j)["exe_name"];
//        win32_data->exe_path         = (*j)["exe_path"];
//        win32_data->priority_class   = (*j)["priority_class"];
//        win32_data->creation_time    = (*j)["creation_time"];
//        win32_data->exit_time        = (*j)["exit_time"];
//        win32_data->kernel_time      = (*j)["kernel_time"];
//        win32_data->user_time        = (*j)["user_time"];
//        win32_data->process_affinity = (*j)["process_affinity"];
//        win32_data->system_affinity  = (*j)["system_affinity"];
//        win32_data->ram_usage        = (*j)["ram_usage"];
//        // win32_data->is_visible_app   = (*j)["is_visible_app"];
//    }
//    catch (...) {
//        return false;
//    }
//    
//    return true;
//}
























// Win32_error win32_get_process_modules(DWORD pid, vector<Win32_process_module>* modules) {
//     HANDLE module_snapshot_handle;
//     MODULEENTRY32 me32;

//     module_snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
//     if (module_snapshot_handle == INVALID_HANDLE_VALUE) {
//         return Win32_error::CreateToolhelp32Snapshot_failed;
//     }

//     // Set the size of the structure before using it.
//     me32.dwSize = sizeof(MODULEENTRY32);

//     if (!Module32First(module_snapshot_handle, &me32)) {
//         CloseHandle(module_snapshot_handle);          
//         return Win32_error::Module32First_failed;
//     }

//     do {
//         Win32_process_module module;
//         module.name     = wchar_to_utf8(me32.szModule);
//         module.exe_path = wchar_to_utf8(me32.szExePath);

//         modules->push_back(module);

//     } while (Module32Next(module_snapshot_handle, &me32));

//     CloseHandle(module_snapshot_handle);
//     return Win32_error::ok;
// }




































































































// pair<int, Win32_error> get_all_active_processe_ids(DWORD* process_ids_arr, size_t arr_len) {
//     DWORD number_of_bytes_returned;
//     int err_code = EnumProcesses(process_ids_arr, 
//                                  (DWORD)(arr_len * sizeof(DWORD)), 
//                                  &number_of_bytes_returned);
    
//     if (err_code == 0) {
//         return pair(0, Win32_error::win32_EnumProcesses_failed);
//     }

//     if (number_of_bytes_returned == arr_len * sizeof(DWORD)) {
//         return pair(0, Win32_error::win32_EnumProcess_buffer_too_small);
//     }

//     return pair(number_of_bytes_returned, Win32_error::ok);     // Success
// }

// pair<int, Win32_error> get_process_path(DWORD process_id, WCHAR* path_buffer, size_t path_buffer_len) {
//     // TODO: see if using ACCESS_ALL is better here.
//     HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, // These specify the access rights
//                                         FALSE,                                       // This is for process creation, we dont need it, so FALSE
//                                         process_id);                                 // Process id to get a handle of
//     if (process_handle == NULL) return pair(0, Win32_error::win32_OpenProcess_failed);
    
//     HMODULE module_handle;              // NOTE(damian): this is the same as HINSTANCE. This is a legacy type, back from 16-bit windows. 
//     DWORD   number_of_bytes_returned;

//     // NOTE(damian): doesnt null terminate. So if overflow, then the name for sure did not fit. No \0.
//     int err_code = EnumProcessModules(process_handle, 
//                                       &module_handle, 
//                                       sizeof(module_handle), 
//                                       &number_of_bytes_returned);
//     if (err_code == 0) {
//         return pair(0, Win32_error::win32_EnumProcessModules_failed);
//     }
//     DWORD path_buffer_new_len =  GetModuleFileNameExW(process_handle, 
//                                                       module_handle, 
//                                                       path_buffer, 
//                                                       path_buffer_len / sizeof(WCHAR));
//     if (path_buffer_new_len == 0) return pair(0, Win32_error::win32_GetModuleFileNameExW_failed);
//     if (path_buffer_new_len == path_buffer_len) return pair(0, Win32_error::win32_GetModuleFileNameExW_buffer_too_small);

//     CloseHandle(process_handle);

//     return pair(path_buffer_new_len, Win32_error::ok);
// }


// // NOTE(damian): this gets all the active process, deals with static buffer overflow. 
// //               if static buffer is too small, uses a bigger dynamic one. 
// //               if a dynamic one was used, pointer to it, allong with the arr length will be returned, 
// //                  else returs nullptr.
// //               DONT FORGET TO DELETE THE DYNAMIC MEMORY LATER
// pair<DWORD*, int> helper_get_all_active_processe_ids(DWORD* process_ids_buffer_on_stack, int process_ids_buffer_on_stack_len) {
//     DWORD* process_ids_buffer_on_heap     = nullptr;
//     int    process_ids_buffer_on_heap_len = process_ids_buffer_on_stack_len * 2;
//     int    bytes_returned                 = -1;  

//     while(true) {
//         std::pair<int, Win32_error> result;
//         if (process_ids_buffer_on_heap == nullptr)
//             result = get_all_active_processe_ids(process_ids_buffer_on_stack, process_ids_buffer_on_stack_len);
//         else 
//             result = get_all_active_processe_ids(process_ids_buffer_on_heap, process_ids_buffer_on_heap_len);

//         if (result.second == Win32_error::win32_EnumProcesses_failed) {
//             std::cout << "Error when getting active windows processes." << std::endl;
//             assert(false);
//         }
//         else if (result.second == Win32_error::win32_EnumProcess_buffer_too_small) {
//             if (process_ids_buffer_on_heap == nullptr) {
//                 process_ids_buffer_on_heap = (DWORD*) malloc(sizeof(DWORD) * process_ids_buffer_on_heap_len);
//             }
//             else {
//                 process_ids_buffer_on_heap_len *= 2;
//                 int bytes_needed                = sizeof(DWORD) * process_ids_buffer_on_heap_len;
//                 process_ids_buffer_on_heap = (DWORD*) realloc(process_ids_buffer_on_heap, bytes_needed);
//                 // TODO(damian): handle realooc NULL.
//             }

//             if (process_ids_buffer_on_heap == NULL) {
//                 std::cout << "Not enought memory to allocate a buffer." << std::endl;
//                 assert(false);
//             }

//         }
//         else if (result.second == Win32_error::ok) {
//             bytes_returned = result.first;
//             break;
//         }
//         else {
//             std::cout << "Unhandeled error." << std::endl;
//                 assert(false);
//         }
//     }
//     assert(bytes_returned != -1);

//     return pair(process_ids_buffer_on_heap, bytes_returned);
// }

// // NOTE(damian): this gets the name for the provided process id, deals with static buffer overflow. 
// //               if static buffer is too small, uses a bigger dynamic one. 
// //               if a dynamic one was used, pointer to it, allong with the name string length will be returned, 
// //                  else returs nullptr.
// //               Error is returned as the thirf element, this is done in order to skip the unaccesible processes.
// //               DONT FORGET TO DELETE THE DYNAMIC MEMORY LATER
// std::tuple<WCHAR*, int, Win32_error> helper_get_process_path(DWORD process_id, WCHAR* stack_process_path_buffer, int stack_process_path_buffer_len) {
//     WCHAR* process_path_buffer_on_heap     = nullptr;
//     int    process_path_buffer_on_heap_len = stack_process_path_buffer_len * 2;
//     int    process_path_str_len            = -1;  

//     while(true) {
//         std::pair<int, Win32_error> result;
//         if (process_path_buffer_on_heap == nullptr)
//             result = get_process_path(process_id, stack_process_path_buffer, stack_process_path_buffer_len);
//         else 
//             result = get_process_path(process_id, process_path_buffer_on_heap, process_path_buffer_on_heap_len);
        
//         if (result.second == Win32_error::win32_OpenProcess_failed) {
//             /*std::cout << "Error, was not able to open a process." << std::endl;
//             assert(false);*/
//             // NOTE(damian): Some procecces are not accasible due to security and some other reasons, 
//             //               so this failing is probably ok.
//             return std::tuple(nullptr, -1, Win32_error::win32_OpenProcess_failed);
//         }
//         else if (result.second == Win32_error::win32_EnumProcessModules_failed) {
//             /*std::cout << "Error, was not able to enum process modules." << std::endl;
//             assert(false);*/
//             // NOTE(damian): Not sure if this failing is ok.
//             return std::tuple(nullptr, -1, Win32_error::win32_EnumProcessModules_failed);
//         }
//         else if (result.second == Win32_error::win32_GetModuleFileNameExW_buffer_too_small) {
//             if (process_path_buffer_on_heap == nullptr) {
//                 process_path_buffer_on_heap = (WCHAR*) malloc(sizeof(WCHAR) * process_path_buffer_on_heap_len);
//             }
//             else {
//                 process_path_buffer_on_heap_len *= 2;
//                 int bytes_needed                 = sizeof(WCHAR) * process_path_buffer_on_heap_len;
//                 process_path_buffer_on_heap = (WCHAR*) realloc(process_path_buffer_on_heap, bytes_needed);
//             }

//             if (process_path_buffer_on_heap == NULL) {
//                 std::cout << "Not enought memory to allocate a buffer." << std::endl;
//                 assert(false);
//             }
//         }
//         else if (result.second == Win32_error::ok) {
//             process_path_str_len = result.first;
//             break;
//         }
//         else {
//             std::cout << "Unhandeled error." << std::endl;
//                 assert(false);
//         }
//     }
//     assert(process_path_str_len != -1);

//     return std::tuple(process_path_buffer_on_heap, process_path_str_len, Win32_error::ok);

// }
    

























