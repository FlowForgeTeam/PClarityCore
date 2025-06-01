#include <iostream>
#include <fstream>
#include "Functions_file_system.h"

void get_all_files_for_path(vector<wstring>* list, fs::path& path, const char* extention) {
    std::error_code error_code;
    for (auto dir_entry : fs::directory_iterator(path, error_code)) {
        if (error_code) break;

        if (fs::is_directory(dir_entry)) {
            auto new_path = dir_entry.path();
            get_all_files_for_path(list, new_path, extention);
        }
        else if (fs::is_regular_file(dir_entry)) {
            bool sohuld_add = true;

            if (extention == nullptr) {
                list->push_back(dir_entry.path().c_str());
            }
            else if (dir_entry.path().extension().string() == extention) {
                list->push_back(dir_entry.path().c_str());
            }

            std::wcout << "Added file: " << path << std::endl;

        }

    }

}

int read_file(const char* file_path, string* text) {
    std::ifstream file(file_path);

    if (!file.is_open()) return 1;
    
    string line;
    while (std::getline(file, line)) {
        text->append(line);
    }

    return 0;
}
