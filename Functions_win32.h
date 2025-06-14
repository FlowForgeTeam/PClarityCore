#pragma once
#include <windows.h>
#include <tchar.h>
#include <psapi.h>
#include <utility>
#include <string>
#include <vector>

using std::pair, std::tuple;
using std::string, std::vector;

// TODO(damian): remove win32 from the enum names.
enum class Win32_error {
    ok,
    win32_EnumProcess_buffer_too_small,
    win32_EnumProcesses_failed,
    win32_OpenProcess_failed,
    win32_EnumProcessModules_failed,
    win32_GetModuleFileNameExW_failed,
    win32_GetModuleFileNameExW_buffer_too_small,
    win32_QueryFullProcessImageNameW,

    CreateToolhelp32Snapshot_failed,
    Process32First_failed,
    Module32First_failed,

    win32_GetRam_failed,

};

// TODO(damian): deal with const wchar* overflow.
// struct Win32_process_module {
//     DWORD  owner_pid;
//     string name;
//     string exe_path;
// };

// TODO(damian): deal with const wchar* overflow.
struct Win32_process_data {
    DWORD      pid;
    DWORD      started_threads;
    DWORD      ppid;
    LONG       base_priority;
    string     exe_name;

    string exe_path;

    DWORD priority_class;

    ULONGLONG creation_time;  
    ULONGLONG exit_time;          
    ULONGLONG kernel_time;    
    ULONGLONG user_time;     

    SIZE_T process_affinity;
    SIZE_T system_affinity;

    SIZE_T ram_usage;

    bool is_visible_app;

    // vector<Win32_process_module> modules;
};

string wchar_to_utf8(const WCHAR* wchar_array);

pair<vector<Win32_process_data>, Win32_error> win32_get_process_data();

tuple<WCHAR*, bool, DWORD, Win32_error> win32_get_path_for_process(HANDLE process_handle, 
                                                                   WCHAR* stack_buffer, 
                                                                   size_t stack_buffer_len);
bool win32_is_process_an_app(HANDLE process_handle, Win32_process_data* data);




// Win32_error win32_get_process_modules(DWORD pid, vector<Win32_process_module>* modules);





// pair<int, Win32_error> get_all_active_processe_ids(DWORD* process_ids_arr, size_t arr_len);
// pair<int, Win32_error> get_process_path(DWORD process_id,  WCHAR* path_buffer, size_t path_buffer_len);

// pair<DWORD*, int> helper_get_all_active_processe_ids(DWORD* process_ids_buffer_on_stack, int process_ids_buffer_on_stack_len);
// tuple<WCHAR*, int, Win32_error> helper_get_process_path(DWORD process_id, WCHAR* stack_process_path_buffer, int stack_process_path_buffer_len);



