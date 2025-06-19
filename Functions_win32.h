#pragma once
#include <windows.h>
#include <tchar.h>
#include <psapi.h>
#include <utility>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using std::pair, std::tuple;
using std::string, std::vector;
using json = nlohmann::json;

// TODO(damian): see is any of these are not used, if so, remove them.
enum class Win32_error {
    ok,

    EnumProcess_buffer_too_small,
    EnumProcesses_failed,
    OpenProcess_failed,
    EnumProcessModules_failed,
    GetModuleFileNameExW_failed,
    GetModuleFileNameExW_buffer_too_small,
    QueryFullProcessImageNameW,

    CreateToolhelp32Snapshot_failed,
    Process32First_failed,
    Module32First_failed,

    win32_GetRam_failed,
};


// TODO(damian): deal with const wchar* overflow.
struct Win32_process_data {
    DWORD      pid;
    DWORD      started_threads;
    DWORD      ppid;
    LONG       base_priority;
    string     exe_name;

    string exe_path;

    DWORD priority_class;

    ULONGLONG process_creation_time;  
    ULONGLONG process_exit_time;          
    ULONGLONG process_kernel_time;    
    ULONGLONG process_user_time;    

    ULONGLONG system_idle_time;
    ULONGLONG system_kernel_time;
    ULONGLONG system_user_time;

    SIZE_T process_affinity;
    SIZE_T system_affinity;

    SIZE_T ram_usage;

    // bool is_visible_app;
};

string wchar_to_utf8(const WCHAR* wchar_array);

pair<vector<Win32_process_data>, Win32_error> win32_get_process_data();

tuple<WCHAR*, bool, DWORD, Win32_error>win32_get_path_for_process(HANDLE process_handle, 
                                                                  WCHAR* stack_buffer, 
                                                                  size_t stack_buffer_len);
bool win32_is_process_an_app(HANDLE process_handle, Win32_process_data* data);

// =============================================================================================

void convert_to_json  (Win32_process_data* win32_data, json* j);
bool convert_from_json(Win32_process_data* win32_data, json* j);


// NOTE(damian): there is a macro inside nlohman for manual serialisation. 
//               kinda works like Swift's Codable trait.
//               not using it, since for types that doesnt implemenet it, i will have to implement it.
//               easier to just json the shit out of it myself. 
//               e.g.: 
//                  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Win32_process_data,
//                      pid, started_threads, ppid, base_priority, exe_name, exe_path,
//                      priority_class, creation_time, exit_time, kernel_time, user_time,
//                      process_affinity, system_affinity, ram_usage, is_visible_app
//                  )














