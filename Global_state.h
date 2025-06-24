#pragma once
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <atomic>
#include <unordered_map>
#include <filesystem>
#include <optional>

#include "Process_data.h"

using nlohmann::json;
using std::vector;
using std::string;
using std::unordered_map;
using std::optional;

namespace G_state {
	
	// Error levels:
	//	1   --> ok
	//  1__ --> Warning
	//  1__ --> FATAL

	enum class Error_type {
		ok = 1,

		// Temporary
		tracked_and_current_process_vectors_share_data, // NOTE(damian): Ttis is used with asserts right now

		// State
		file_with_tracked_processes_doesnt_exist        = 10,			
		sessions_data_folder_doesnt_exist               = 11,					
		trying_to_track_the_same_process_more_than_once = 12,	
		trying_to_untrack_a_non_tracked_process         = 13,					 
		no_csv_file_for_tracked_process                 = 14, 			
		err_logs_file_was_not_present                   = 15, 
		folder_for_process_icons_doesnt_exist 			= 16,

		// Startup
		startup_json_tracked_processes_file_parsing_failed    = 100,    
		startup_json_tracked_processes_file_invalid_structure = 101, 

		// TODO(damian): remove
		// Network`
		tcp_initialisation_failed, // Not on the data thread

		// File system
		error_reading_a_file, // TODO(damian): what do we do with this?
		filesystem_error,	  // TODO(damian): what do we do with this?
	};

	struct Error {
		Error_type type;
		std::string message;

		Error(Error_type type);
		Error(Error_type type, const char *message);
		~Error() = default;
	};
	
	extern Error err;

	// TODO(damian): move this out of here into their own separate namespace.
	// == Constants =============================================
	extern const char* path_file_error_logs;
	extern const char* path_file_tracked_processes;
	extern const char* path_dir_sessions;
	// ===========================================================

	// == Data data ==============================================
	extern vector<Process_data> currently_active_processes;
	extern vector<Process_data> tracked_processes;
	// ===========================================================

	extern G_state::Error set_up_on_startup();
	extern G_state::Error update_state();

	extern G_state::Error add_process_to_track     (string *path);
	extern G_state::Error remove_process_from_track(string *path);

	// ===========================================================

	// This is the data, created by update state, for the client to use thread safely. 
	// The data is only created when it is requested via need_data bool flag.
	namespace Client_data {
		extern bool need_data;
		struct Data {
			vector<Process_data> copy_currently_active_processes;
			vector<Process_data> copy_tracked_processes;	
		};
		extern optional<Data> maybe_data;
	}

	// This is global system data. 
	namespace System_info {
		extern long long up_time;
		extern SYSTEMTIME system_time;
	}

}
