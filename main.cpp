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

static bool data_thread_running = true;
static std::thread client_thread;

static void handle_fatal_error(G_state::Error* err) {
	Client::fatal_error = *err;
	client_thread.join();
	data_thread_running = false;
}

int main() {
	G_state::Error init_err(G_state::Error_type::ok);
	
	init_err = G_state::set_up_on_startup();
		
	if (init_err.type == G_state::Error_type::ok) {
		init_err = G_state::update_state();
	}

	if (init_err.type != G_state::Error_type::ok) { std::cout << "Fatal error on startup. \n" << std::endl; }

	client_thread = std::thread(Client::client_thread); // This starts right away.

	if (init_err.type != G_state::Error_type::ok) {
		handle_fatal_error(&init_err);
		std::cout << "Failed on startup. \n" << std::endl; 
	}
	else { std::cout << "Done setting up. \n" << std::endl; }
	
	// ==================================================================

	int n = 1;
	while (data_thread_running) {
		std::chrono::seconds sleep_length(G_state::Settings::n_sec_between_state_updates);
		std::this_thread::sleep_for(sleep_length);
		
		std::cout << " ------------ N : " << n++ << " ------------ " << std::endl;

		G_state::Error err = G_state::update_state();
		if (err.type != G_state::Error_type::ok) {
			handle_fatal_error(&err);
			break;
		}

		// Getting the first command has not yet been handled
		auto p_to_request    = Client::request_queue.begin(); 
		bool unhandled_found = false; 
		for (; p_to_request != Client::request_queue.end(); ++p_to_request) {
			if (!p_to_request->handled) {
				unhandled_found = true;
				break;
			}
		}

		// TODO(damian): why the fuck does main add new processes, g_state should be doing it when it updates its state. 
		if (unhandled_found) {
			Track_request*      track       = std::get_if<Track_request>  (&p_to_request->request.variant);
			Untrack_request*    untrack     = std::get_if<Untrack_request>(&p_to_request->request.variant);
			Change_update_time* change_time = std::get_if<Change_update_time>(&p_to_request->request.variant);

			G_state::Error maybe_err(G_state::Error_type::ok);
			if (track != nullptr) {
				maybe_err = G_state::add_process_to_track(&track->path);
			}
			else if (untrack != nullptr) {
				maybe_err = G_state::remove_process_from_track(&untrack->path);
			}
			else if (change_time != nullptr) {
				maybe_err = G_state::update_settings_file(change_time->duration_in_sec);
			}
			else {
				assert(false);
			}

			if (maybe_err.type != G_state::Error_type::ok) {
				handle_fatal_error(&maybe_err);
				break;
			}

			p_to_request->handled = true;
		}



	}

	std::cout << "\n";
	std::cout << "Finished running." << std::endl;

	return 0;
}



















