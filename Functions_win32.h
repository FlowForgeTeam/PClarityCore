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

struct Win32_process_data {
    DWORD      pid;
    DWORD      started_threads;
    DWORD      ppid;
    LONG       base_priority;
    string     exe_name;

    string exe_path;

    string product_name;

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

    bool has_image;

};

void wchar_to_utf8(WCHAR* wstr, string* str);

pair<vector<Win32_process_data>, Win32_error> win32_get_process_data();

tuple<WCHAR*, bool, DWORD, Win32_error>win32_get_path_for_process(HANDLE process_handle, 
                                                                  WCHAR* stack_buffer, 
                                                                  size_t stack_buffer_len);

bool SaveIconToPath(PBYTE icon, DWORD buf, string* output_path);
bool FileExists    (string* path);

bool win32_is_process_an_app(HANDLE process_handle, Win32_process_data* data);

// =============================================================================================














