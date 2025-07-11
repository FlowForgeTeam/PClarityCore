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

		// State
		startup_file_with_tracked_processes_doesnt_exist        = 10,			 // W
		startup_sessions_folder_doesnt_exist                    = 11,				  // W	
		
		startup_no_dir_for_specific_tracked_process				= 14,  // W
		startup_no_overall_csv_file_for_tracked_process			= 15,  // W
		startup_no_current_session_csv_file_for_tracked_process = 16,  // W
		
		err_logs_file_was_not_present                   = 17,  // W
		
		startup_folder_for_process_icons_doesnt_exist 			= 18, // W
		
		startup_invalid_values_inside_json					    = 19,
		
		startup_setting_file_doesnt_exists 						= 20,
		
		startup_json_tracked_processes_file_parsing_failed      = 100,    
		startup_json_tracked_processes_file_invalid_structure   = 101,
		
		
		startup_json_settings_file_parsing_failed      = 102,    
		startup_json_settings_file_invalid_structure   = 103,
		
		process_current_csv_file_contains_more_than_1_record = 114,

		startup_filesystem_is_all_fucked_up = 104,
		runtime_filesystem_is_all_fucked_up = 105,
		
		trying_to_track_the_same_process_more_than_once         = 12,	
		trying_to_untrack_a_non_tracked_process                 = 13,
		
		os_error = 123,

		runtime_logics_failed = 124,

		CreateToolhelp32Snapshot_failed = 125,
		Process32First_failed = 126,
		
		malloc_fail,

		tracked_and_current_process_vectors_share_data,

		// NOTE(damian): It would be better to remove this, but its fine.
		tcp_initialisation_failed, 

	};

	struct Error {
		Error_type type;
		std::string message;

		Error(Error_type type);
		Error(Error_type type, const char *message);
		~Error() = default;
	};
	
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
		extern Data data;
	}

	// This is global system data. 
	namespace Static_system_info {
		
	}

	namespace Dynamic_system_info {
		extern long long up_time;
		extern SYSTEMTIME system_time;

		extern optional<Win32_system_times> prev_system_times;
		extern optional<Win32_system_times> new_system_times;
	}

	namespace Settings {
		extern const uint32_t default_n_sec_between_state_updates;
		extern       uint32_t n_sec_between_state_updates;

		extern const uint32_t default_n_sec_between_csv_updates;
		extern       uint32_t n_sec_between_csv_updates; 
	}

}
