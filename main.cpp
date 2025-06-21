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
#include "Request.h"
#include "Handlers_for_main.h"

// NOTE(damian): bool represent wheather the command has alredy been handled.
// TOOD(damian): maybe add this to the global state.

static void handle_global_err(G_state::Error* err);

static G_state::Error g_err(G_state::Error_type::ok);

int main() {
	g_err = G_state::set_up_on_startup();
	
	G_state::update_state();
	std::cout << "Done setting up. \n" << std::endl;

	std::thread client (Main::client_thread); // This starts right away.

	int n = 1;
	while (Main::running) {
		std::chrono::seconds sleep_length(1);
		std::this_thread::sleep_for(sleep_length);
		
		std::cout << " ------------ N : " << n++ << " ------------ " << std::endl;

		G_state::Error err = G_state::update_state();
		if (err.type != G_state::Error_type::ok) {
			handle_global_err(&err);
		}

		// Getting the first command has not yet been handled
		auto p_to_request    = Main::request_queue.begin(); 
		bool unhandled_found = false; 
		for (; p_to_request != Main::request_queue.end(); ++p_to_request) {
			if (!p_to_request->handled) {
				unhandled_found = true;
				break;
			}
		}

		if (unhandled_found) {
			Track_request*   track   = std::get_if<Track_request>  (&p_to_request->request.variant);
			Untrack_request* untrack = std::get_if<Untrack_request>(&p_to_request->request.variant);

			if (track != nullptr) {
				G_state::add_process_to_track(&track->path);
			}
			else if (untrack != nullptr) {
				G_state::remove_process_from_track(&untrack->path);
			}
			else {
				assert(false);
			}

			p_to_request->handled = true;
		}

	}

	return 0;
}

void handle_global_err(G_state::Error* err) {



}


















