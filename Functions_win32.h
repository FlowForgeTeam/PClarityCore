#pragma once
#include <windows.h>
#include <tchar.h>
#include <psapi.h>
#include <utility>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <nlohmann/json.hpp>

#include "Get_process_exe_icon.h"

using std::tuple;
using std::string, std::vector;
using std::optional;
using json = nlohmann::json;

namespace fs = std::filesystem;

// Forward decl for a part of G_state namespace
namespace G_state {
    struct Error; 
}

struct Win32_snapshot_data {
    DWORD   pid;
    DWORD   started_threads;
    DWORD   ppid;
    LONG    base_priority;
    string  exe_name;
};

struct Win32_process_times {
    ULONGLONG creation_time;  
    ULONGLONG exit_time;          
    ULONGLONG kernel_time;    
    ULONGLONG user_time;
};

struct Win32_system_times {
    ULONGLONG idle_time;
    ULONGLONG kernel_time;
    ULONGLONG user_time;
};

struct Win32_affinities {
    SIZE_T process_affinity;
    SIZE_T system_affinity;
};

struct Win32_process_data {
    Win32_snapshot_data             snapshot;
    string                          exe_path;
    optional<string>                product_name;
    optional<DWORD>                 priority_class;
    Win32_process_times             process_times;   
    optional<Win32_affinities>      affinities;
    optional<SIZE_T>                ram_usage;
    bool                            has_image;
};
// NOTE(damian): 
//      Process_times are not optional, even tho they might be. They are not the same reason that exe_path isnt optional.
//      We use them to identidy processes. Because of that, we cant be working on active processes that dont allow us to id them correcly.

tuple< G_state::Error, 
       vector<Win32_process_data>, 
       optional<Win32_system_times>,
       ULONGLONG,
       SYSTEMTIME > win32_get_process_data();

void wchar_to_utf8(WCHAR* wstr, string* str);

// =============================================================================================














