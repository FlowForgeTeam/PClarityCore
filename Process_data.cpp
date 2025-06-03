#include "Process_data.h"

// NOTE(damian): this was gpt generated, need to make sure its is correct. 
std::string wchar_to_utf8(const WCHAR* wstr) {
    if (!wstr) return "";

    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";

    std::string result(size - 1, 0); // size-1 to exclude null terminator
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], size, nullptr, nullptr);

    return result;
}

Process_data::Process_data(wstring name, int time_spent) {
    this->name        = wchar_to_utf8(name.c_str());
    this->time_spent  = std::chrono::seconds(time_spent);
    this->is_active   = false;
    this->was_updated = false;
    this->start       = std::chrono::steady_clock::now();
}

Process_data::Process_data(string name, int time_spent) {
    this->name        = name;
    this->time_spent  = std::chrono::seconds(time_spent);
    this->is_active   = false;
    this->was_updated = false;
    this->start       = std::chrono::steady_clock::now();
}

Process_data::~Process_data() {}

// Updating a process as if its new state is active
void Process_data::update_active() {
    if (this->is_active == false) {
        this->is_active = true;
        this->start     = std::chrono::steady_clock::now();
    }
    else {
        auto elapsed_time = std::chrono::steady_clock::now() - this->start;
        this->time_spent += std::chrono::duration_cast<std::chrono::seconds>(elapsed_time);
        this->start       = std::chrono::steady_clock::now();
    }
    this->was_updated = true;
}

// Updating a process as if its new state is in_active
void Process_data::update_inactive() {
    if (this->is_active == true) {
        this->is_active   = false;
        auto elapsed_time = std::chrono::steady_clock::now() - this->start;
        this->time_spent  += std::chrono::duration_cast<std::chrono::seconds>(elapsed_time);
    }
    else {
        // Nothing here
    }

    this->was_updated = true;
}

// =====================================================================

void string_into_wstring(const string* str, const wstring* wstr) {


}

int convert_from_json(const json* data, Process_data* process_data) {
    if (   data->contains("process_name")
        && data->contains("time_spent")
    ) {
        process_data->name       = (*data)["process_name"];
        process_data->time_spent = std::chrono::seconds((*data)["time_spent"]);
        process_data->is_active  = false;
        process_data->start      = std::chrono::steady_clock::now();

        return 0;
    }
    else {
        return 1;
    }
}

void convert_to_json(json* data, const Process_data* process_data) {
    (*data)["process_name"] = process_data->name;
    (*data)["time_spent"]   = process_data->time_spent.count();
}


