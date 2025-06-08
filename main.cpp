#include <nlohmann/json.hpp>
#include "Functions_file_system.h"
#include "Functions_networking.h"
#include "Functions_win32.h"
#include "Global_state.h"
#include "Commands.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <assert.h>
#include <vector>
#include <string>
#include <thread>
#include <list>

// NOTE(damian): bool represent wheather the command has alredy been 
struct Command_status {
	bool    handled;
	Command command;
};
std::list<Command_status> command_queue; 

void client_thread();

void wait_for_client_to_connect();

void handle_report  (Report_command*   command);
void handle_quit    (Quit_command*     commnad);
void handle_shutdown(Shutdown_command* commnad);
void handle_track   (Track_command*    commnad);
void handle_untrack (Untrack_command*  commnad);

// TODO(damian): move these somewhere better, here for now.
static bool   running         = true;
static SOCKET client_socket         ;
static bool   need_new_client = true;
static string error_message         ;

int main() {

	G_state::set_up_on_startup();
	G_state::update_state();
	std::cout << "Done setting up. \n" << std::endl;

	std::thread client (&client_thread); // This starts right away.

	int n = 1;
	while (running) {
		std::chrono::duration<int> sleep_length(3);
		std::this_thread::sleep_for(sleep_length);
		
		std::cout << " ------------ N : " << n++ << "------------ " << std::endl;

		G_state::update_state();

		// Getting the first command has not yet been handled
		auto p_to_command    = command_queue.begin(); 
		bool unhandled_found = false; 
		for (; p_to_command != command_queue.end(); ++p_to_command) {
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

// TODO(damian): handle out of bounds for message_parts. 

// TOOD(damian): think about namespacing function for both thread,
//				 just to be more sure, that thread use shated data as little as possible.  

void client_thread() {
	while (running) {
		if (need_new_client) {
			wait_for_client_to_connect();
			need_new_client = false;
		}

		// Clearing out the command queue.
		for (auto it = command_queue.begin(); it != command_queue.end();) {
			if (it->handled)
				it = command_queue.erase(it);
			else
				++it;
		}

		// NOTE: dont forget to maybe extend it if needed, or error if the message is to long or something.
		// NOTE(damian): recv doesnt null terminate the string buffer, 
		//	             so terminating it myself, to then be able to use strcmp.

		char receive_buffer[512];
		int  receive_buffer_len = 512;

		int n_bytes_returned = recv(client_socket, receive_buffer, receive_buffer_len, NULL); // TODO(damian): add a timeout here.
		receive_buffer[n_bytes_returned] = '\0';

		std::cout << "Received a message of " << n_bytes_returned << " bytes." << std::endl;
		std::cout << "Message: " << "'" << receive_buffer << "'" << "\n" << std::endl;

		// Parsing the request.
		std::pair<Command, bool> result = command_from_json(receive_buffer);
		if (!result.second) {
			const char* message = "Invalid command";
			int send_err_code = send(client_socket, message, strlen(message), NULL);
			if (send_err_code == SOCKET_ERROR) {
				// TODO: handle
			}
		}
		else {
			switch (result.first.type) {
			case Command_type::report: {
				handle_report(&result.first.data.report);
			} break;

			case Command_type::quit: {
				handle_quit(&result.first.data.quit);
			} break;

			case Command_type::shutdown: {
				handle_shutdown(&result.first.data.shutdown);
			} break;

			case Command_type::track: {
				handle_track(&result.first.data.track);
			} break;

			case Command_type::untrack: {
				handle_untrack(&result.first.data.untrack);
			} break;

			default: {
				assert(false);
			} break;
			}
		}

		// Preserve the new state
		std::ofstream data_file("data.json");

		vector<json> processes_as_jsons;
		for (Process_data& process_data : G_state::tracked_processes) {
			json temp;
			convert_to_json(&temp, &process_data);
			processes_as_jsons.push_back(temp);
		}
		json j_overall_data;
		j_overall_data["processes_to_track"] = processes_as_jsons;

		data_file << j_overall_data.dump(4) << std::endl;
		data_file.close();

		std::cout << "Saved data to file. \n" << std::endl;



	}
}

void wait_for_client_to_connect() {
	std::cout << "Waiting for a new connection with a client." << std::endl;
	pair<SOCKET, G_state::Error> result = initialise_tcp_connection_with_client();
	if (result.second == G_state::Error::ok) {
		client_socket = result.first;
	}
	else assert(false);
	std::cout << "Client connected. \n" << std::endl;
}

void handle_report(Report_command* report) {
	vector<json> j_tracked;
	for (Process_data& data : G_state::tracked_processes) {
		json temp;
		convert_to_json(&temp, &data);
		j_tracked.push_back(temp);
	}

	vector<json> j_cur_active;
	for (Process_data& data : G_state::currently_active_processes) {
		json temp;
		convert_to_json(&temp, &data);
		j_cur_active.push_back(temp);
	}

	json global_json;
	global_json["tracked"]          = j_tracked;
	global_json["currently_active"] = j_cur_active;

	std::string message_as_str = global_json.dump(4);

	int send_err_code = send(client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}

	std::cout << "Message handling: " << message_as_str << "\n" << std::endl;
}

void handle_quit(Quit_command* quit) {
	string message = "Client disconnected.";
	
	int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}

	closesocket(client_socket);

	std::cout << "Message handling: " << message << "\n" << std::endl;

	need_new_client = true;
}

void handle_shutdown(Shutdown_command* shutdown) {
	string message = "Stopped traking.";

	int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}

	closesocket(client_socket);

	std::cout << "Message handling: " << message << "\n" << std::endl;
	
	running = false;
}

void handle_track(Track_command* track) {
	Command command;
	command.type = Command_type::track;

	// Correctly construct the union member using placement new
	new (&command.data.track) Track_command(*track);

	Command_status status;
	status.handled = false;
	status.command = command;

	command_queue.push_back(status);

	string message = "The procided process will be removed to the list of tracke processes.";
	int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}
}

void handle_untrack(Untrack_command* untrack) {
	Command command;
	command.type = Command_type::untrack;

	// Correctly construct the union member using placement new
	new (&command.data.untrack) Untrack_command(*untrack);

	Command_status status;
	status.handled = false;
	status.command = command;

	command_queue.push_back(status);

	string message = "The procided process will be removed to the list of tracke processes.";
	int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
	 	// TODO: handle
	}
}

















