#pragma once
#include <vector>
#include <string>
#include <filesystem>

using std::vector, std::string, std::wstring;

namespace fs = std::filesystem;

void get_all_files_for_path(vector<wstring>* list, fs::path& path, const char* extention = nullptr);
int  read_file(const char* file_name, string* text);


