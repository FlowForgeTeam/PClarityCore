#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <stdio.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <psapi.h> // For PROCESS_MEMORY_COUNTERS

//  Forward declarations:
std::map<DWORD, PROCESSENTRY32> GetAllProcesses();
void printError(TCHAR const* msg);
SIZE_T GetProcessMemoryUsage(DWORD processID);

int main(void)
{
    GetAllProcesses();
    return 0;
}

SIZE_T GetProcessMemoryUsage(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    if (hProcess == NULL) return 0;

    PROCESS_MEMORY_COUNTERS pmc;
    SIZE_T memoryUsage = 0;

    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        memoryUsage = pmc.WorkingSetSize;
    }

    CloseHandle(hProcess);
    return memoryUsage;
}

std::map<DWORD, PROCESSENTRY32> GetAllProcesses()
{
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;

    // Take a snapshot of all processes in the system.
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE)
    {
        return std::map<DWORD, PROCESSENTRY32>();
    }

    // Set the size of the structure before using it.
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Retrieve information about the first process,
    // and exit if unsuccessful
    if (!Process32First(hProcessSnap, &pe32))
    {
        CloseHandle(hProcessSnap);         
        return std::map<DWORD, PROCESSENTRY32>();
    }

    // Maps to store parent-child relationships
    std::map<DWORD, PROCESSENTRY32> processes; // Map of all processes by ID

    // All the processes
    do
    {
        processes[pe32.th32ProcessID] = pe32;
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    return processes;
}