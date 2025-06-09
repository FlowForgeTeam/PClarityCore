#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <string>

class process_cpu_usage {
private:
    HANDLE h_process;
    ULARGE_INTEGER last_cpu;
    ULARGE_INTEGER last_sys_cpu;
    ULARGE_INTEGER last_user_cpu;
    DWORD num_processors;
    DWORD process_id;
    bool initialized;

    static const short measurement_interval = 500;

public:
    process_cpu_usage(DWORD pid) : process_id(pid), initialized(false) {
        // Open the process
        h_process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id);
        if (h_process == NULL) {
            std::cerr << "Could not open process. Error code: " << GetLastError() << std::endl;
            return;
        }

        // Get number of processors
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        num_processors = sys_info.dwNumberOfProcessors;
    }

    ~process_cpu_usage() {
        if (h_process != NULL)
            CloseHandle(h_process);
    }

    // Get CPU usage as a percentage (0-100)
    double get_usage(int measurement_interval_ms = measurement_interval) {
        if (h_process == NULL) return -1.0;

        FILETIME create_time, exit_time, kernel_time, user_time;
        if (!GetProcessTimes(h_process, &create_time, &exit_time, &kernel_time, &user_time)) {
            return -1.0;
        }

        ULARGE_INTEGER now;
        FILETIME file_time;
        GetSystemTimeAsFileTime(&file_time);
        now.LowPart = file_time.dwLowDateTime;
        now.HighPart = file_time.dwHighDateTime;

        // Convert FILETIME to ULARGE_INTEGER
        ULARGE_INTEGER kernel_time_value, user_time_value;
        kernel_time_value.LowPart = kernel_time.dwLowDateTime;
        kernel_time_value.HighPart = kernel_time.dwHighDateTime;
        user_time_value.LowPart = user_time.dwLowDateTime;
        user_time_value.HighPart = user_time.dwHighDateTime;

        if (!initialized) {
            // First call - initialize values and wait for the specified interval
            last_cpu = now;
            last_sys_cpu = kernel_time_value;
            last_user_cpu = user_time_value;
            initialized = true;

            // Wait for measurement interval
            Sleep(measurement_interval_ms);

            // Recursive call to get the actual measurement
            return get_usage(0);  // Pass 0 to avoid waiting again
        }

        // Calculate CPU usage
        ULARGE_INTEGER sys_kernel_diff, user_kernel_diff;
        sys_kernel_diff.QuadPart = kernel_time_value.QuadPart - last_sys_cpu.QuadPart;
        user_kernel_diff.QuadPart = user_time_value.QuadPart - last_user_cpu.QuadPart;

        ULARGE_INTEGER total_cpu_diff;
        total_cpu_diff.QuadPart = sys_kernel_diff.QuadPart + user_kernel_diff.QuadPart;

        // Time passed in 100-nanosecond intervals
        ULARGE_INTEGER time_diff;
        time_diff.QuadPart = now.QuadPart - last_cpu.QuadPart;

        // Update last values for next call
        last_cpu = now;
        last_sys_cpu = kernel_time_value;
        last_user_cpu = user_time_value;

        // Calculate percentage (0-100)
        if (time_diff.QuadPart > 0) {
            double percent = (total_cpu_diff.QuadPart * 100.0) / time_diff.QuadPart;
            return percent / num_processors;
        }

        return 0.0;
    }

    // Static method to get CPU usage for current process
    static double get_current_process_usage(int measurement_interval_ms = measurement_interval) {
        process_cpu_usage usage(GetCurrentProcessId());
        return usage.get_usage(measurement_interval_ms);
    }
};