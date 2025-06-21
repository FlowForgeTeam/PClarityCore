#include <fstream>      
#include <iostream>
#include <Windows.h>
#include <tlhelp32.h>
#include <nlohmann/json.hpp>
#include <cassert>
#pragma comment(lib, "version") // Linking the dll

#include "Functions_win32.h"
#include "Get_process_exe_icon.h"

#include <cwchar>

namespace fs = std::filesystem;

const std::string default_icon_path = "icons\\";

// TODO(damian): if all these win32 function fail for the same reason then skipping this process is fine,
//               but if they dont, we would be better of getting some public info for the process, 
//               rather then getting none. (Just something to think about).

static bool get_process_product_name(HANDLE process_handle, WCHAR* exe_path_buffer, string* product_name);
static bool get_process_times       (HANDLE process_handle, Win32_process_data* data);
static bool store_process_icon_image(Win32_process_data* data);

 
// TODO(damian): check parameters for the win32 string functions.
void wchar_to_utf8(WCHAR* wstr, string* str) {
    if (!wstr) *str = "";

    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) *str = "";

    str->resize(size); // size to include the null terminator
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str->data(), size, nullptr, nullptr);
}

std::pair<vector<Win32_process_data>, Win32_error> win32_get_process_data() {
    // Take a snapshot of all processes in the system.
    HANDLE process_shapshot_handle;
    PROCESSENTRY32 pe32;

    process_shapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (process_shapshot_handle == INVALID_HANDLE_VALUE) {
        return pair(vector<Win32_process_data>(), Win32_error::CreateToolhelp32Snapshot_failed);
    }

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
        if (process_handle == NULL) { continue; }

        // 1. Getting snapshot data
        Win32_process_data data = {0};
        data.pid             = pe32.th32ProcessID;
        data.ppid            = pe32.th32ParentProcessID;
        wchar_to_utf8(pe32.szExeFile, &data.exe_name);
        data.base_priority   = pe32.pcPriClassBase;
        data.started_threads = pe32.cntThreads;

        // 2. Getting process path
        const size_t path_stack_buffer_len = 140; 
        WCHAR path_stack_buffer[path_stack_buffer_len];
        tuple<WCHAR*, bool, DWORD, Win32_error> result = win32_get_path_for_process(process_handle, 
                                                                                    path_stack_buffer, 
                                                                                    path_stack_buffer_len);
        WCHAR*      path_buffer    = std::get<0>(result); // Stack or Heap, for convinient usage
        bool        is_buffer_heap = std::get<1>(result);
        DWORD       buffer_len     = std::get<2>(result);
        Win32_error err_code_2     = std::get<3>(result);
        
        if (err_code_2 != Win32_error::ok) { 
            if (is_buffer_heap) free(path_buffer);
            continue;
        }

        wchar_to_utf8(path_buffer, &data.exe_path);
        
        // 3. Getting process product name (ONLY IN ENGLISH)
        bool err_code_3 = get_process_product_name(process_handle, path_buffer, &data.product_name);

        if (is_buffer_heap) free(path_buffer);
        if (!err_code_3) {
            CloseHandle(process_handle);
            continue;
        }

        // =====================================================
        // TODO(damian): when having a process snapshot, 
        //               need to figure out how we can tell if the process has ended to the pid is invlid now.
        // NOTE(damian): using GetExitCodeProcess is the answer. 
        // =====================================================

        // 4. 
        DWORD priority = GetPriorityClass(process_handle);
        if (priority == 0) {
            CloseHandle(process_handle); 
            continue;
        }
        data.priority_class = priority;

        // 5. Getting times for process
        bool err_code_4 = get_process_times(process_handle, &data);
        if (!err_code_4) {
            CloseHandle(process_handle); 
            continue;
        }

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

        // 7.
        bool err_code_7 = store_process_icon_image(&data);
        if (!err_code_7) {
            CloseHandle(process_handle);
            continue;
        }

        // GPU
        // SOME OTHER STUFF (PROCESS LASSO TYPE SHIT)
        
        process_data_vec.push_back(data);

        CloseHandle(process_handle);

    } while (Process32Next(process_shapshot_handle, &pe32));

    CloseHandle(process_shapshot_handle);

    return pair(process_data_vec, Win32_error::ok);
}

tuple<WCHAR*, bool, DWORD, Win32_error> win32_get_path_for_process(HANDLE process_handle,
                                                                   WCHAR* stack_buffer,
                                                                   size_t stack_buffer_len
) {
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

bool SaveIconToPath(PBYTE icon, DWORD buf, std::string output_path) {
    try {
        // Extract directory path and create it if it doesn't exist
        fs::path filePath(output_path);
        fs::path dirPath = filePath.parent_path();

        if (!dirPath.empty() && !fs::exists(dirPath)) {
            std::cout << "Creating directory: " << dirPath << std::endl;
            fs::create_directories(dirPath);
        }

        std::ofstream file(output_path, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<char*>(icon), buf);
            file.close();
            std::cout << "Icon saved to: " << output_path << std::endl;
            return true;
        }
        else {
            std::cout << "Failed to create output file: " << output_path << std::endl;
            return false;
        }
    }
    catch (const fs::filesystem_error& ex) {
        std::cout << "Filesystem error: " << ex.what() << std::endl;
        return false;
    }
}

bool FileExists(const std::string& path) {
    DWORD fileAttr = GetFileAttributesA(path.c_str());
    return (fileAttr != INVALID_FILE_ATTRIBUTES &&
        !(fileAttr & FILE_ATTRIBUTE_DIRECTORY));
}

    static bool is_visible_app = false; 
    static BOOL CALLBACK win32_is_process_an_app_callback(HWND window_handle, LPARAM lParam);
bool win32_is_process_an_app(HANDLE process_handle, Win32_process_data* data) {
    BOOL err_code = EnumWindows(win32_is_process_an_app_callback, data->pid);

    if (is_visible_app) {
        is_visible_app = false;
        return true;
    }

    return false;
}


// =============================================================================================

bool get_process_product_name(HANDLE process_handle, WCHAR* exe_path_buffer, string* product_name) {
    DWORD file_version_size_bytes = GetFileVersionInfoSizeW(exe_path_buffer, NULL);
    if (file_version_size_bytes == 0) { return false; }

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
            assert(false);
            // TODO(damian): handle better.
        }
        file_info_err           = GetFileVersionInfoW(exe_path_buffer, NULL, file_version_size_bytes, file_version_data);
        heap_used               = true;
    }

    if (file_info_err == 0) {
        if (heap_used) free(file_version_data);
        return false;        
        // TODO(damian): handle better.
    }

    WCHAR* product_name_engl = nullptr;
    UINT   product_name_len_engl = 0;
    int translation_err = VerQueryValueW(file_version_data, 
                                         L"\\StringFileInfo\\040904B0\\ProductName", // NOTE(damian): (0409 is hexadec for 1033, 04B0 is hexedec for 1200) and read the comment bellow.
                                         (void**) &product_name_engl, 
                                         &product_name_len_engl);
    if (translation_err == 0) {
        if (heap_used) free(file_version_data);
        return false;
        // TODO(damian): handle better.
    }  

    wchar_to_utf8(product_name_engl, product_name);

    return true;

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
    //     // TODO(damian): handle better.
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

bool get_process_times(HANDLE process_handle, Win32_process_data* data) {
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
    data->process_creation_time     = process_creation_time.QuadPart;

    ULARGE_INTEGER process_exit_time;
    process_exit_time.LowPart  = process_last_exit_f_time.dwLowDateTime;
    process_exit_time.HighPart = process_last_exit_f_time.dwHighDateTime;
    data->process_exit_time     = process_exit_time.QuadPart;

    ULARGE_INTEGER process_kernel_time;
    process_kernel_time.LowPart  = process_kernel_f_time.dwLowDateTime;
    process_kernel_time.HighPart = process_kernel_f_time.dwHighDateTime;
    data->process_kernel_time     = process_kernel_time.QuadPart;

    ULARGE_INTEGER process_user_time;
    process_user_time.LowPart  = process_user_f_time.dwLowDateTime;
    process_user_time.HighPart = process_user_f_time.dwHighDateTime;
    data->process_user_time     = process_user_time.QuadPart;

    // Getting system times
    FILETIME system_idle_f_time, system_kernel_f_time, system_user_f_time;
    if (GetSystemTimes(&system_idle_f_time, 
                        &system_kernel_f_time, 
                        &system_user_f_time) == 0
    ) {
        return false;
    }

    ULARGE_INTEGER system_idle_time;
    system_idle_time.LowPart  = system_idle_f_time.dwLowDateTime;
    system_idle_time.HighPart = system_idle_f_time.dwHighDateTime;
    data->system_idle_time     = system_idle_time.QuadPart;

    ULARGE_INTEGER system_kernel_time;
    system_kernel_time.LowPart  = system_kernel_f_time.dwLowDateTime;
    system_kernel_time.HighPart = system_kernel_f_time.dwHighDateTime;
    data->system_kernel_time     = system_kernel_time.QuadPart;

    ULARGE_INTEGER system_user_time;
    system_user_time.LowPart  = system_user_f_time.dwLowDateTime;
    system_user_time.HighPart = system_user_f_time.dwHighDateTime;
    data->system_user_time     = system_user_time.QuadPart;

    return true;
}

bool store_process_icon_image(Win32_process_data* data) {
    // Creating image if none exists
    // NOTE(andrii): default_icon_path - the path to which the icons are saved.
    // The directory "icons" in the same directory by default
    if (!data->has_image) {
        std::string icon_path = default_icon_path + data->exe_name + ".ico";

        if (!FileExists(icon_path)) {
            DWORD bufLen = 0;
            PBYTE icon = get_exe_icon_from_file_utf8(data->exe_path.c_str(), TRUE, &bufLen);

            // NOTE(andrii): commented code was for debugging, could implement better logging
            // if (data.exe_name == "zen.exe") {
            //     std::cout << "Zen icon found for: " << data.exe_name << std::endl;
            // }

            if (icon) {
                if (!SaveIconToPath(icon, bufLen, icon_path)) {
                    //std::cerr << "Failed to save icon to path: " << icon_path << std::endl;
                }
                else {
                    data->has_image = true;
                }
                free(icon);
            }
            else {
                //std::cerr << "Failed to get icon for process: " << data->exe_name << std::endl;
            }
        }
        else {
            data->has_image = true;
        }
    }

    return true;
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



























































































































