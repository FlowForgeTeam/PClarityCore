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

void data_thread();
void client_thread();

void wait_for_client_to_connect();

void handle_report  ();
void handle_quit    ();
void handle_shutdown();
void handle_track   (std::string* path);
void handle_untrack (std::string* path);

// TODO(damian): move these somewhere better, here for now.
static bool   running = true;
static SOCKET client_socket;
static bool need_new_client = true;

int main() {

 	G_state::set_up_on_startup();
	G_state::update_state();
	std::cout << "Done setting up. \n" << std::endl;

	std::thread client (&client_thread); // This starts right away.

	while (running) {
		std::chrono::duration<int> sleep_length(3);
		std::this_thread::sleep_for(sleep_length);
	}

	return 0;
}

// TODO(damian): handle out of bounds for message_parts. 

void data_thread() {
	// This just gets the data. 
}

// TOOD(damian): think about namespacing function for both thread,
//				 just to be more sure, that thread use shated data as little as possible.  

void client_thread() {
	while (running) {
		if (need_new_client) {
 			wait_for_client_to_connect();
 			need_new_client = false;
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
		json j_message = json::parse(receive_buffer);
		
		const char* invalid_command_message = nullptr;

		if (   !j_message.contains("command_id")
			|| !j_message.contains("extra")) 
		{ 
			invalid_command_message = "Invalid message structure."; 
		}

		int command_id = j_message["command_id"];
		std::optional<Command> opt = command_from_int(command_id);
		
		if (!opt.has_value()) {
			invalid_command_message = "Invalid command.";
		}

		Command command = opt.value();
		switch (command) {
			case Command::report: {
				handle_report();
			} break;
			
			case Command::quit: {
				handle_quit();
			} break;

			case Command::shutdown: {
				handle_shutdown();
			} break;

			// case Command::track: {
			// 	json j_extra = j_message["extra"];
			// 	if (!j_extra.contains("path")) {
			// 		invalid_command_message = "Invalid command. Track has to have path inside extra.";
			// 		break;
			// 	}
			// 	string path = j_extra["path"];

			// 	handle_track(&path);
			// } break;

			// case Command::untrack: {
			// 	json j_extra = j_message["extra"];
			// 	if (!j_extra.contains("path")) {
			// 		invalid_command_message = "Invalid command. Untrack has to have path inside extra.";
			// 		break;
			// 	}
			// 	string path = j_extra["path"];

			// 	handle_untrack(&path);
			// } break;

			default: {
				assert(false);
			} break;
	
		}

		if (invalid_command_message != nullptr) {
			int send_err_code = send(client_socket, invalid_command_message, strlen(invalid_command_message), NULL);
			if (send_err_code == SOCKET_ERROR) {
				// TODO: handle
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
	pair<SOCKET, Network_error> result = initialise_tcp_connection_with_client();
	if (result.second == Network_error::ok) {
		client_socket = result.first;
	}
	else assert(false);
	std::cout << "Client connected. \n" << std::endl;
}

void handle_report() {
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

void handle_quit() {
	string message = "Client disconnected.";
	
	int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}

	closesocket(client_socket);

	std::cout << "Message handling: " << message << "\n" << std::endl;

	need_new_client = true;
}

void handle_shutdown() {
	string message = "Stopped traking.";

	int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}

	closesocket(client_socket);

	std::cout << "Message handling: " << message << "\n" << std::endl;
	
	running = false;
}

void handle_track(std::string* path) {
	G_state::Error err_code = G_state::add_process_to_track(path);

	if (err_code == G_state::Error::ok) {
		string message = "Started tracking a new process.";
		int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
		if (send_err_code == SOCKET_ERROR) {
			// TODO: handle
		}
		return;
	}

	if (err_code == G_state::Error::trying_to_track_the_same_process_more_than_once) {
		string message = "Cant add the track process twice, it is alredy beeing tracked.";
		int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
		if (send_err_code == SOCKET_ERROR) {
			// TODO: handle
		}
		return;
	} 

	string message = "Started tracking a process.";
	int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}

	// TODO(damian): maybe add some cmd printing here. 

	std::cout << "Unhandled G_state::Error error." << std::endl;
	assert(false);

}

void handle_untrack(std::string* path) {
	G_state::Error err_code = G_state::remove_process_from_track(path);

	if (err_code == G_state::Error::trying_to_untrack_a_non_tracked_process) {
		string message = "The provided process was not beeing tracked, so nothing was removed.";
		int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
		if (send_err_code == SOCKET_ERROR) {
			// TODO: handle
		}
	}
	else if (err_code != G_state::Error::ok) {
		std::cout << "Unhandles error." << std::endl;
		assert(false);
	}

	string message = "Stopped tracking a process.";
	int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}
	
}
















