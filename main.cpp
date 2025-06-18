#include <fstream>
#include <iostream>
#include <filesystem>
#include <assert.h>
#include <vector>
#include <string>
#include <thread>
#include <list>
#include <nlohmann/json.hpp>

#include "Functions_file_system.h"
#include "Functions_networking.h"
#include "Functions_win32.h"
#include "Global_state.h"
#include "Commands.h"
#include "Handlers_for_main.h"

// NOTE(damian): bool represent wheather the command has alredy been handled.
// TOOD(damian): maybe add this to the global state.

int main() {
	G_state::set_up_on_startup();
	G_state::update_state();
	std::cout << "Done setting up. \n" << std::endl;

	std::thread client (Main::client_thread); // This starts right away.

	int n = 1;
	while (Main::running) {
		std::chrono::duration<int> sleep_length(1);
		std::this_thread::sleep_for(sleep_length);
		
		std::cout << " ------------ N : " << n++ << " ------------ " << std::endl;

		G_state::Error err = G_state::update_state();
		if (err.type != G_state::Error_type::ok) {
			std::cout << "Update state error: " << "'" << err.message << "'" << std::endl;
			exit(1);
		}

		// Getting the first command has not yet been handled
		auto p_to_command    = Main::command_queue.begin(); 
		bool unhandled_found = false; 
		for (; p_to_command != Main::command_queue.end(); ++p_to_command) {
			if (!p_to_command->handled) {
				unhandled_found = true;
				break;
			}
		}

		if (unhandled_found) {
			switch (p_to_command->command.type) {
				case Command_type::track: {
					G_state::add_process_to_track(&p_to_command->command.data.track.path);
				} break;
				
				case Command_type::untrack: {
					G_state::remove_process_from_track(&p_to_command->command.data.untrack.path);
				} break;

				default: {
					assert(false);
				} break;
			}

			p_to_command->handled = true;
		}

	}

	return 0;
}
  


















