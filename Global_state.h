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
		startup_file_with_tracked_processes_doesnt_exist        = 10,			
		startup_sessions_folder_doesnt_exist                    = 11,					
		trying_to_track_the_same_process_more_than_once         = 12,	
		trying_to_untrack_a_non_tracked_process                 = 13,

		startup_no_dir_for_specific_tracked_process				= 14,
		startup_no_overall_csv_file_for_tracked_process			= 15,
		startup_no_current_session_csv_file_for_tracked_process = 16,

		startup_err_logs_file_was_not_present                   = 17, 
		
		startup_folder_for_process_icons_doesnt_exist 			= 18,

		startup_invalid_values_inside_json					    = 19,

		startup_setting_file_doesnt_exists 						= 20,
		
		// Startup
		startup_json_tracked_processes_file_parsing_failed      = 100,    
		startup_json_tracked_processes_file_invalid_structure   = 101,
		
		startup_json_settings_file_parsing_failed      = 102,    
		startup_json_settings_file_invalid_structure   = 103,
		
		startup_filesystem_is_all_fucked_up = 104,
		runtime_filesystem_is_all_fucked_up = 105,
		
		os_error = 106,

		// TODO(damian): remove
		// Network`
		tcp_initialisation_failed, // Not on the data thread
	};

	struct Error {
		Error_type type;
		std::string message;

		Error(Error_type type);
		Error(Error_type type, const char *message);
		~Error() = default;
	};
	
	//extern Error err;

	// TODO(damian): move this out of here into their own separate namespace.
	// == Constants =============================================
	extern const char* path_file_error_logs;
	extern const char* path_file_tracked_processes;
	extern const char* path_dir_sessions;
	extern const char* path_dir_process_icons;
	extern const char* path_file_settings; 
	
	extern const char* process_session_csv_file_header;

	extern const char* csv_file_name_for_overall_sessions_for_process;
	extern const char* csv_file_name_for_current_session_for_process;
	// ===========================================================

	// == Data data ==============================================
	extern vector<Process_data> currently_active_processes;
	extern vector<Process_data> tracked_processes;
	// ===========================================================

	extern G_state::Error set_up_on_startup();
	extern G_state::Error update_state();

	extern G_state::Error add_process_to_track     (string *path);
	extern G_state::Error remove_process_from_track(string *path);

	extern G_state::Error update_settings_file(uint32_t n_sec);

	extern void convert_path_to_windows_filename(string* path_to_be_changed);

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
	namespace Static_system_info {
		
	}

	namespace Dynamic_system_info {
		extern long long up_time;
		extern SYSTEMTIME system_time;
	}

	namespace Settings {
		extern const uint32_t default_n_sec_between_updates;
		extern       uint32_t n_sec_between_updates;
	}

}
