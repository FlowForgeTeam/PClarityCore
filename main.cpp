#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <thread>
#include <list>
#include <nlohmann/json.hpp>

#include "Functions_networking.h"
#include "Functions_win32.h"
#include "Global_state.h"
#include "Request.h"
#include "Handlers_for_main.h"

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
	std::cout << "Started the client thread. \n" << std::endl;

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
		
		std::cout << " ------------ Data thread loop : " << n++ << " ------------ " << std::endl;

		if (!Client::client_running) { // NOTE(damian): something weird happend to the client that made it stop itself.
			std::cout << "Client broke down, GG. \n" << std::endl;
			break;
		}

		G_state::Error err = G_state::update_state();
		if (err.type != G_state::Error_type::ok) {
			handle_fatal_error(&err);
			
			std::cout << "Error when updating state. GG. \n" << std::endl;
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

		// TODO(damian): why the fuck does main add new processes, G_state should be doing it when it updates its state. 
		if (unhandled_found) {
			Track_request*      track       = std::get_if<Track_request>  (&p_to_request->request.variant);
			Untrack_request*    untrack     = std::get_if<Untrack_request>(&p_to_request->request.variant);
			Change_update_time_request* change_time = std::get_if<Change_update_time_request>(&p_to_request->request.variant);

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
				Error err(Error_type::runtime_logics_failed, "A request was given to the data thread to be handled, but there is no handler for that type of request. ");
				handle_fatal_error(&err);
				
				std::cout << "A propogated request to the data thread by client is not handled inside main. GG. \n" << std::endl;
				break;
			}

			if (maybe_err.type != G_state::Error_type::ok) {
				handle_fatal_error(&maybe_err);

				std::cout << "Error caught. \n" << std::endl;
				break;
			}

			p_to_request->handled = true;
		}

	}

	std::cout << "\n";
	std::cout << "Finished running. \n" << std::endl;

	return 0;
}



















