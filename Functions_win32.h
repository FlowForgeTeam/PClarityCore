#pragma once
#include <windows.h>
#include <tchar.h>
#include <psapi.h>

// NOTE(damian 03.06.2025): This will probably be the way we return errors here.
//                          std::pair with the second value being the error value.
#include <utility>
using std::pair;

enum class Win32_error {
    ok,
    win32_EnumProcess_buffer_too_small,
    win32_EnumProcesses_failed,
    win32_OpenProcess_failed,
    win32_EnumProcessModules_failed,
    win32_GetModuleFileNameExW_failed,
    win32_GetModuleFileNameExW_buffer_too_small,

};

pair<int, Win32_error> get_all_active_processe_ids(DWORD* process_ids_arr, size_t arr_len);
pair<int, Win32_error> get_process_path(DWORD process_id,  WCHAR* path_buffer, size_t path_buffer_len);

// NOTE(damian): this works, but not yet ussed, so commented out.
// int get_process_path(DWORD process_id, WCHAR* path_buffer, size_t path_buffer_len);

