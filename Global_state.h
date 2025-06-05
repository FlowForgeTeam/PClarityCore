#pragma once
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

#include "Process_data.h"

using std::vector, std::string;
using nlohmann::json;

namespace G_state {
	enum class Error {
		ok,
		trying_to_track_the_same_process_more_than_once,
	};

	extern const char* data_file_name;
	extern vector<Process_data> process_data_vec;

	extern void set_up_on_startup();
	extern void update_state();

	// TODO(damian): see if maybe passing apointer or just string for name and path is better
	G_state::Error add_process_to_track(string* path);

}

