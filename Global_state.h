#pragma once
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <atomic>
#include <unordered_map>
#include <filesystem>

#include "Process_data.h"

using nlohmann::json;
using std::vector, std::string;

using std::unordered_map;

namespace G_state
{
	enum class Error_type
	{
		ok,

		// State
		file_with_tracked_processes_doesnt_exist,
		sessions_data_folder_doesnt_exist,
		trying_to_track_the_same_process_more_than_once,
		trying_to_untrack_a_non_tracked_process,
		tracked_and_current_process_vectors_share_data,
		process_data_invalid_behaviour,
		no_csv_file_for_tracked_process,

		// json
		json_parsing_failed,
		json_invalid_strcture,

		// Network
		tcp_initialisation_failed,

		// Other
		unhandled_error_caught,
		error_reading_a_file,

		filesystem_error,

		// TODO: Temp placeholder
		other,
	};

	struct Error
	{
		Error_type type;
		std::string message;

		Error(Error_type type);
		Error(Error_type type, const char *message);
		~Error() = default;
	};

	// TODO(damian): change the name.
	// struct Node {
	// 	Process_data*  process;
	// 	vector<Node*> child_processes_nodes;
	// };

	extern const char* file_path_with_tracked_processes;
	extern const char* sessions_dir_path;
	extern vector<Process_data> currently_active_processes;
	extern vector<Process_data> tracked_processes;

	// extern vector<Node*> tree;

	extern G_state::Error set_up_on_startup();
	extern G_state::Error update_state();

	extern G_state::Error add_process_to_track(string *path);
	extern G_state::Error remove_process_from_track(string *path);
	// extern void create_tree(); // TODO(damian): change the name.
	// extern void create_copy_tree();

}
