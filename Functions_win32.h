#pragma once
#include <windows.h>
#include <tchar.h>
#include <psapi.h>

int  get_all_active_processe_ids(DWORD* process_ids_arr, size_t arr_len);
bool see_if_process_is_active(DWORD* process_ids_arr, size_t arr_len, const TCHAR* process_name);
bool is_process_active_by_name(const TCHAR* process_name);