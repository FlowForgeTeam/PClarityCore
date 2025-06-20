#include <fstream>      
#include <iostream>
#include <Windows.h>
#include <tlhelp32.h>
#include <cassert>

#include "Functions_win32.h"

// TODO(damian): if all these win32 function fail for the same reason then skipping this process is fine,
//               but if they dont, we would be better of getting some public info for the process, 
//               rather then getting none. (Just something to think about).

// TODO: maybe have these weird win32 typedefs briefly documented somewhere here.

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

        // 2.
        const size_t stack_buffer_len = 512;
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
        FILETIME process_creation_time;
        FILETIME process_last_exit_time;
        FILETIME process_kernel_time;
        FILETIME process_user_time;
        if (GetProcessTimes(process_handle, &process_creation_time, &process_last_exit_time, &process_kernel_time, &process_user_time) == 0) {
            CloseHandle(process_handle);
            continue;
        }

        // NOTE(damian): dont know if we need to store the decoded vecrion.
        // SYSTEMTIME creation_time;
        // FileTimeToSystemTime(&process_creation_time, &creation_time);
        // data.creation_time = creation_time;

        // SYSTEMTIME exit_time;
        // FileTimeToSystemTime(&process_last_exit_time, &exit_time);
        // data.exit_time = exit_time;

        ULARGE_INTEGER creation_time;
        creation_time.LowPart  = process_creation_time.dwLowDateTime;
        creation_time.HighPart = process_creation_time.dwHighDateTime;
        data.creation_time = creation_time.QuadPart;

        ULARGE_INTEGER exit_time;
        exit_time.LowPart  = process_last_exit_time.dwLowDateTime;
        exit_time.HighPart = process_last_exit_time.dwHighDateTime;
        data.exit_time = exit_time.QuadPart;

        ULARGE_INTEGER kernel_time;
        kernel_time.LowPart  = process_kernel_time.dwLowDateTime;
        kernel_time.HighPart = process_kernel_time.dwHighDateTime;
        data.kernel_time = kernel_time.QuadPart;

        ULARGE_INTEGER user_time;
        user_time.LowPart  = process_user_time.dwLowDateTime;
        user_time.HighPart = process_user_time.dwHighDateTime;
        data.user_time = user_time.QuadPart;

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


        // CPU time
        // GPU
        // SOME OTHER STUFF (PROCESS LASSO TYPE SHIT)
        
        // NOTE(damian): the name for the process might also be retrived via QueryFullProcessImageNameA.
        //               currently we get it from the first module inside the process modules vector.
        //               based on the docs, the first module retrived by the Module32First function 
        //               always contais data for the process whos modules are retrived.

        data.is_visible_app = win32_is_process_an_app(process_handle, &data);

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
    DWORD path_len;
    path_len = GetProcessImageFileName(process_handle, stack_buffer, stack_buffer_len);

    // Stack buffer was long enought
    if (path_len != 0) {
        return tuple(stack_buffer, false, path_len, Win32_error::ok);
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
            heap_buffer = (WCHAR*) realloc(heap_buffer, heap_buffer_len);
        }

        if (heap_buffer == NULL) {
            assert(false);
            // TODO(damian): handle better.
        }

        path_len = GetProcessImageFileName(process_handle, heap_buffer, heap_buffer_len);
        
        if (path_len != 0) { // Allocated succesfully
            return tuple(heap_buffer, true, path_len, Win32_error::ok);
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
    

























