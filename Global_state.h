#pragma once
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

#include "Process_data.h"

using std::vector, std::string;
using nlohmann::json;

namespace G_state {
	extern const char* data_file_name;
	extern vector<Process_data> process_data_vec;

	extern void set_up_on_startup();

	extern void update_state();
}

