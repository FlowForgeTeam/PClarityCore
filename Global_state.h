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

		// State
		data_file_doesnt_exist,
		trying_to_track_the_same_process_more_than_once,
		trying_to_untrack_a_non_tracked_process,
		tracked_and_current_process_vectors_share_data,
		process_data_invalid_behaviour,

		// json
		json_parsing_failed,
		json_invalid_strcture,

		// Network
		WSASTARTUP_failed,
		getaddrinfo_failed,
		bind_failed,
		listen_failed,
		accept_failed,
		closesocket_failed,

		// Other
		unhandled_error_caught,

		// Temp placeholder
		other,
	};

	extern const char* data_file_name;
	extern vector<Process_data> currently_active_processes;
	extern vector<Process_data> tracked_processes;

	extern G_state::Error set_up_on_startup();
	extern G_state::Error update_state();

	extern G_state::Error add_process_to_track     (string* path);
	extern G_state::Error remove_process_from_track(string* path);
}

