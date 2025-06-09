#pragma once
#include <windows.h>

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
    // Constructor that takes a process ID
    process_cpu_usage(DWORD pid);

    // Destructor
    ~process_cpu_usage();

    // Get CPU usage as a percentage (0-100)
    double get_usage(int measurement_interval_ms = measurement_interval);

    // Static method to get CPU usage for current process
    static double get_current_process_usage(int measurement_interval_ms = measurement_interval);
};