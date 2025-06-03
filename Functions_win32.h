#pragma once
#include <windows.h>
#include <tchar.h>
#include <psapi.h>

int get_all_active_processe_ids(DWORD* process_ids_arr, size_t arr_len);
int get_process_name(DWORD process_id,  WCHAR* name_buffer, size_t name_buffer_len);
int get_process_path(DWORD process_id, WCHAR* path_buffer, size_t path_buffer_len);

// NOTE(damian): these are not great, but work. Commented out, since i will change them later.
// bool see_if_process_is_active(DWORD* process_ids_arr, size_t arr_len, const WCHAR* process_name);
// bool is_process_active_by_name(const WCHAR* process_name);