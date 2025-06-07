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
		trying_to_untrack_a_non_tracked_process,
	};

	extern const char* data_file_name;
	extern vector<Process_data> currently_active_processes;
	extern vector<Process_data> tracked_processes;

	extern void set_up_on_startup();
	extern void update_state();

	// TODO(damian): see if maybe passing a pointer or just string for name and path is better
	G_state::Error add_process_to_track(string* path);
	G_state::Error remove_process_from_track(string* path);



}

