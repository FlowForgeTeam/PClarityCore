#include <fstream>      
#include <iostream>
#include <Windows.h>
#include <tlhelp32.h>
#pragma comment(lib, "version") // Linking the dll
#include <cwchar>

#include "Functions_win32.h"
#include "Global_state.h"

namespace fs = std::filesystem;

static pair<Error, bool> get_process_product_name(HANDLE process_handle, WCHAR* exe_path_buffer, string* product_name);
static bool get_process_times                    (HANDLE process_handle, Win32_process_times* times);
static optional<Win32_system_times> get_system_times();

static Error store_process_icon_image(Win32_process_data* data);
//static Error save_icon_to_path(PBYTE icon, DWORD buf, fs::path* file_path);


static tuple< optional<WCHAR*>, bool, DWORD, Error > win32_get_path_for_process(HANDLE process_handle,
                                                                                WCHAR* stack_buffer,
                                                                                size_t stack_buffer_len);

tuple< G_state::Error, 
       vector<Win32_process_data>, 
       optional<Win32_system_times>,
       ULONGLONG,
       SYSTEMTIME > win32_get_process_data() {

    // Take a snapshot of all processes in the system.
    HANDLE process_shapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (process_shapshot_handle == INVALID_HANDLE_VALUE) {
        return tuple(Error(Error_type::CreateToolhelp32Snapshot_failed),
                     vector<Win32_process_data>(),
                     std::nullopt,
                     ULONGLONG{},
                     SYSTEMTIME{}); 
    }
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(process_shapshot_handle, &pe32)) {
        CloseHandle(process_shapshot_handle);
        return tuple(Error(Error_type::Process32First_failed),
                     vector<Win32_process_data>(),
                     std::nullopt,
                     ULONGLONG{},
                     SYSTEMTIME{}); 
    }

    vector<Win32_process_data> process_data_vec;
    do {
        // Opening a handle, all the data for the process is kept until all the handles are released.
        HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
            FALSE,
            pe32.th32ProcessID);
        if (process_handle == NULL) { continue; }
            
        Win32_process_data data = {};
            
        // 1. Getting snapshot data
        Win32_snapshot_data snapshot = {};
        snapshot.pid             = pe32.th32ProcessID;
        snapshot.ppid            = pe32.th32ParentProcessID;

        string temp_name;
        wchar_to_utf8(pe32.szExeFile, &temp_name);
        snapshot.exe_name = std::move(temp_name);

        snapshot.base_priority   = pe32.pcPriClassBase;
        snapshot.started_threads = pe32.cntThreads;

        data.snapshot = std::move(snapshot);

        // 2. Getting process path
        const size_t path_stack_buffer_len = 140; 
        WCHAR path_stack_buffer[path_stack_buffer_len];
        tuple<optional<WCHAR*>, bool, DWORD, Error> result = win32_get_path_for_process(process_handle, 
                                                                                        path_stack_buffer, 
                                                                                        path_stack_buffer_len);
        optional<WCHAR*> path_buffer    = std::get<0>(result); // Stack or Heap, for convinient usage. nullopt if the func call failed for some win32 reasons.
        bool             is_buffer_heap = std::get<1>(result);
        DWORD            buffer_len     = std::get<2>(result);
        Error            err_2          = std::get<3>(result);
        
        if (err_2.type != Error_type::ok) { 
            if (is_buffer_heap) free(path_buffer.value());
            CloseHandle(process_handle); 
            CloseHandle(process_shapshot_handle);
            return tuple(err_2, 
                         vector<Win32_process_data>(), 
                         Win32_system_times{},
                         ULONGLONG{},
                         SYSTEMTIME{}); 
        }

        if (!path_buffer.has_value()) { 
            if (is_buffer_heap) free(path_buffer.value());
            CloseHandle(process_handle); 
            continue;
        }
        
        string temp_path;
        wchar_to_utf8(path_buffer.value(), &temp_path);
        data.exe_path = std::move(temp_path);
        
        // 3. Getting process product name (ONLY IN ENGLISH)
        string temp_product_name;
        pair<Error, bool> maybe_prod_name = get_process_product_name(process_handle, path_buffer.value(), &temp_product_name);
        
        if (is_buffer_heap) { free(path_buffer.value()); path_buffer = nullptr; }

        if (maybe_prod_name.first.type != Error_type::ok) { 
            CloseHandle(process_handle); 
            CloseHandle(process_shapshot_handle);
            return tuple(err_2, 
                         vector<Win32_process_data>(), 
                         Win32_system_times{},
                         ULONGLONG{},
                         SYSTEMTIME{}); 
        }

        if (maybe_prod_name.second) { data.product_name = std::move(temp_product_name); } 
        else                        { data.product_name = std::nullopt; }

        // 4. 
        DWORD priority = GetPriorityClass(process_handle);
        if (priority == 0) {
            data.priority_class = std::nullopt;
        }
        else {
            data.priority_class = priority;
        }

        // 5. Getting times for process
        Win32_process_times process_times = {};
        bool err_code_4 = get_process_times(process_handle, &process_times);
        if (err_code_4) {
            data.process_times = process_times;
        }
        else {
            CloseHandle(process_handle);
            continue;
        }

        // 5. 
        SIZE_T process_affinity_mask;
        SIZE_T system_affinity_mask;
        if(GetProcessAffinityMask(process_handle, &process_affinity_mask, &system_affinity_mask) != 0) {
            Win32_affinities affinities = {};
            affinities.process_affinity = process_affinity_mask;
            affinities.system_affinity  = system_affinity_mask;
            data.affinities = affinities;
        }
        else {
            data.affinities = std::nullopt;
        }
        
        // 6.
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(process_handle, &pmc, sizeof(pmc)) != 0) {
            data.ram_usage = pmc.WorkingSetSize;
        }
        else {
            data.ram_usage = std::nullopt;
        }

        // 7.
        Error err_7 = store_process_icon_image(&data);
        if (err_7.type != Error_type::ok) { 
            CloseHandle(process_handle);
            CloseHandle(process_shapshot_handle);
            return tuple(err_7, 
                         vector<Win32_process_data>(), 
                         Win32_system_times{},
                         ULONGLONG{},
                         SYSTEMTIME{});
        }

        // GPU
        // SOME OTHER STUFF (PROCESS LASSO TYPE SHIT)
        
        process_data_vec.push_back(data);

        CloseHandle(process_handle);

    } while (Process32Next(process_shapshot_handle, &pe32));
    CloseHandle(process_shapshot_handle);

    optional<Win32_system_times> system_times = get_system_times();

    ULONGLONG pc_up_time = GetTickCount64();

    SYSTEMTIME sys_time = {};
    GetSystemTime(&sys_time);

    return tuple(G_state::Error(G_state::Error_type::ok), 
                 process_data_vec, 
                 system_times,
                 pc_up_time,
                 sys_time);
}


void wchar_to_utf8(WCHAR* wstr, string* str) {
    if (!wstr) *str = "";

    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) *str = "";

    str->resize(size - 1); // size to include the null terminator
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &(str->operator[](0)), size, nullptr, nullptr);
}

// =============================================================================================

// Pointer to string, is_heap, string len, is_err.
tuple< optional<WCHAR*>, bool, DWORD, Error > win32_get_path_for_process(HANDLE process_handle,
                                                            WCHAR* stack_buffer,
                                                            size_t stack_buffer_len
) {
    DWORD path_len = stack_buffer_len;
    BOOL  err      = QueryFullProcessImageNameW(process_handle, NULL, stack_buffer, &path_len); // Does null terminate.

    if (err != NULL) { // Success
        return tuple(stack_buffer, false, path_len, Error(Error_type::ok));
    }
    else if (GetLastError() != 122) { // NOTE(damian): 122 is code for insufficient buffer size.
        return tuple(std::nullopt, false, path_len, Error(Error_type::ok));
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

        if (heap_buffer == NULL) { return tuple(std::nullopt, false, path_len, Error(Error_type::malloc_fail)); }

        DWORD path_len = heap_buffer_len;
        BOOL  err      = QueryFullProcessImageNameW(process_handle, NULL, heap_buffer, &path_len);
        
        if (err != NULL) { // Success
            return tuple(heap_buffer, true, path_len, Error(Error_type::ok));
        }
        else if (GetLastError() != 122) { // NOTE(damian): 122 is code for insufficient buffer size. Not it, so some other error.
            return tuple(std::nullopt, false, path_len, Error(Error_type::ok));
        }

    }

}

pair<Error, bool> get_process_product_name(HANDLE process_handle, WCHAR* exe_path_buffer, string* product_name) {
    DWORD file_version_size_bytes = GetFileVersionInfoSizeW(exe_path_buffer, NULL);
    if (file_version_size_bytes == 0) { return pair(Error(Error_type::ok), false); }

    const size_t stack_alloc_len   = 2500;
    uint8_t      stack_alloc[stack_alloc_len];

    uint8_t* file_version_data = stack_alloc;
    int      file_info_err     = 0;
    bool     heap_used         = false;
    
    // NOTE(damian): tested the average size returned by GetFileVersionInfoSizeW, it was around 2100 bytes.
    
    if (file_version_size_bytes <= stack_alloc_len) {
        file_info_err = GetFileVersionInfoW(exe_path_buffer, NULL, file_version_size_bytes, file_version_data);
    }
    else {
        file_version_data = (uint8_t*) malloc(file_version_size_bytes);
        if (file_version_data == NULL) {
            return pair(Error(Error_type::malloc_fail), false);
        }
        heap_used               = true;
        file_info_err           = GetFileVersionInfoW(exe_path_buffer, NULL, file_version_size_bytes, file_version_data);
    }

    // TODO: think about a way to not have to get product name every time maybe. Allocating every time is ridiculus.

    if (file_info_err == 0) {
        if (heap_used) free(file_version_data);
        return pair(Error(Error_type::ok), false);        
    }

    WCHAR* product_name_engl = nullptr;
    UINT   product_name_len_engl = 0;
    int translation_err = VerQueryValueW(file_version_data, 
                                         L"\\StringFileInfo\\040904B0\\ProductName", // NOTE(damian): (0409 is hexadec for 1033, 04B0 is hexedec for 1200) and read the comment bellow.
                                         (void**) &product_name_engl, 
                                         &product_name_len_engl);
    if (translation_err == 0) {
        if (heap_used) free(file_version_data);
        return pair(Error(Error_type::ok), false);        
    }  

    wchar_to_utf8(product_name_engl, product_name);

    if (heap_used) free(file_version_data);
    return pair(Error(Error_type::ok), true);        

    // This iterated over the langs and codepages a process supports.
    // NOTE(damian): dont know what to if the process supports more than 1 lang for the proouctname.
    //               i would probably store all of them and then the client picks 1, 
    //               problem is that i would need to send data for the client about the langcodes,
    //               but there is no func that cann convert a lang code to some win32 enum value or string representing a lang name.
    //
    //               even lang codes are not documented, so just hardcoding the english once in
    //               English lang --> 1033, Unicode_codepage --> 1200. 
    //               There are some link, but good fucking luck finding them.
    //               Codepages  --> https://learn.microsoft.com/en-us/windows/win32/intl/code-page-identifiers 
    //               Lang_codes --> https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-lcid/70feba9f-294e-491e-b6eb-56532684c37f
    //                  (Downloda the file and scroll til lyou find a table with lang ids) 

    // struct Lang_and_codepage {
    //     WORD lang;
    //     WORD codepage;
    // };

    // Lang_and_codepage* arr_lang_and_codepage = nullptr;
    // UINT  lang_and_codepage_arr_size_bytes   = 0;
    // int translation_err = VerQueryValueW(file_version_data, 
    //                                      L"\\VarFileInfo\\Translation", 
    //                                      (void**) &arr_lang_and_codepage, 
    //                                      &lang_and_codepage_arr_size_bytes);
    // if (translation_err == 0) {
    //     CloseHandle(process_handle); 
    //     continue;
    // }  
    // for (int i = 0; i<lang_and_codepage_arr_size_bytes / sizeof(Lang_and_codepage); ++i) {
    //     WCHAR subBlock[256];
    //     swprintf_s(subBlock, L"\\StringFileInfo\\%04X%04X\\ProductName", 
    //             arr_lang_and_codepage[i].lang, arr_lang_and_codepage[i].codepage);
    //
    //     WCHAR* product_name      = nullptr; 
    //     UINT   product_name_len  = 0;
    //
    //     int product_name_err = VerQueryValueW(file_version_data, 
    //                                      subBlock, 
    //                                      (void**) &product_name, 
    //                                      &product_name_len);
    //     if (product_name_err == 0) {
    //         CloseHandle(process_handle);
    //         break;
    //     }
    // }
}

bool get_process_times(HANDLE process_handle, Win32_process_times* times) {
    // Getting process times
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
        return false;
    }

    ULARGE_INTEGER process_creation_time;
    process_creation_time.LowPart  = process_creation_f_time.dwLowDateTime;
    process_creation_time.HighPart = process_creation_f_time.dwHighDateTime;
    times->creation_time     = process_creation_time.QuadPart;

    ULARGE_INTEGER process_exit_time;
    process_exit_time.LowPart  = process_last_exit_f_time.dwLowDateTime;
    process_exit_time.HighPart = process_last_exit_f_time.dwHighDateTime;
    times->exit_time     = process_exit_time.QuadPart;

    ULARGE_INTEGER process_kernel_time;
    process_kernel_time.LowPart  = process_kernel_f_time.dwLowDateTime;
    process_kernel_time.HighPart = process_kernel_f_time.dwHighDateTime;
    times->kernel_time     = process_kernel_time.QuadPart;

    ULARGE_INTEGER process_user_time;
    process_user_time.LowPart  = process_user_f_time.dwLowDateTime;
    process_user_time.HighPart = process_user_f_time.dwHighDateTime;
    times->user_time     = process_user_time.QuadPart;    

    return true;
}

optional<Win32_system_times> get_system_times() {
    FILETIME system_idle_f_time, system_kernel_f_time, system_user_f_time;
    if (GetSystemTimes(&system_idle_f_time, 
                        &system_kernel_f_time, 
                        &system_user_f_time) == 0
    ) {
        return std::nullopt;
    }
    Win32_system_times times;

    ULARGE_INTEGER system_idle_time;
    system_idle_time.LowPart  = system_idle_f_time.dwLowDateTime;
    system_idle_time.HighPart = system_idle_f_time.dwHighDateTime;
    times.idle_time     = system_idle_time.QuadPart;

    ULARGE_INTEGER system_kernel_time;
    system_kernel_time.LowPart  = system_kernel_f_time.dwLowDateTime;
    system_kernel_time.HighPart = system_kernel_f_time.dwHighDateTime;
    times.kernel_time     = system_kernel_time.QuadPart;

    ULARGE_INTEGER system_user_time;
    system_user_time.LowPart  = system_user_f_time.dwLowDateTime;
    system_user_time.HighPart = system_user_f_time.dwHighDateTime;
    times.user_time     = system_user_time.QuadPart;

    return optional<Win32_system_times>(times);
}

Error store_process_icon_image(Win32_process_data* data) {
    // Checking validity of the file system.
    std::error_code err_code;
    bool dir_exists = fs::is_directory(G_state::path_dir_process_icons, err_code);
    if (!dir_exists) {
        if (err_code != std::errc::no_such_file_or_directory) { return Error(Error_type::os_error); }
        else { return Error(Error_type::runtime_filesystem_is_all_fucked_up); }
    }

    // Creating image if doesnt exist.
    if (!data->has_image) {
        fs::path icon_path;
        icon_path.append(G_state::path_dir_process_icons);
        
        string process_path_copy = data->exe_path;
        G_state::convert_path_to_windows_filename(&process_path_copy);

        icon_path.append(process_path_copy);
        icon_path.replace_extension(".ico");

        std::error_code file_err_code;
        bool file_exists = fs::is_regular_file(icon_path, file_err_code);
        if (file_exists) {
            data->has_image = true;
        }
        else {
            if (file_err_code != std::errc::no_such_file_or_directory) { return Error(Error_type::os_error); }

            DWORD bufLen = 0;
            PBYTE icon = get_exe_icon_from_file_utf8(data->exe_path.c_str(), TRUE, &bufLen);
            if (icon) { 
                std::fstream icon_file(icon_path, std::ios::out | std::ios::binary);
                if (!icon_file.is_open()) { return Error(Error_type::os_error); }
                icon_file.write((char* ) icon, bufLen); 
                icon_file.close();

                data->has_image = true;
            }
            else {
                data->has_image = false; 
                //std::cerr << "Failed to get icon for process: " << data->exe_name << std::endl;
            }
            free(icon);
        }
    }

    return Error(Error_type::ok);
}







































