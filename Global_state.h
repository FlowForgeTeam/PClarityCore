#pragma once
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

#include "Process_data.h"

using nlohmann::json;
using std::vector, std::string;

namespace G_state {
	enum class Error_type {
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
		tcp_initialisation_failed,

		// Other
		unhandled_error_caught,
		error_reading_a_file,

		// TODO: Temp placeholder
		other,
	};

	struct Error {
		Error_type type;
		std::string message;

		Error(Error_type type);
		Error(Error_type type, const char *message);
		~Error() = default;
	};

	extern const char* data_file_path;
	extern vector<Process_data> currently_active_processes;
	extern vector<Process_data> tracked_processes;

	extern G_state::Error set_up_on_startup();
	extern G_state::Error update_state();

	extern G_state::Error add_process_to_track(string *path);
	extern G_state::Error remove_process_from_track(string *path);
}
