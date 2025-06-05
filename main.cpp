#include <thread>
#include <nlohmann/json.hpp>
#include <assert.h>
#include "Functions_file_system.h"
#include "Functions_networking.h"
#include "Functions_win32.h"
#include "Global_state.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

// NOTE(damian): Messages have the following strcture:
//				 <command> <args1> <arg2> ... 

// TODO(damian): 
// 		Messages to handle:
//			-- (get_complete_data)      --> get complete data
//			-- (disconnect)				--> disconnect the current client
//			-- (stop)					--> stop the tracking completely
//
//			-- (start_tracking_process) --> add a process to be tracked
// 			-- (stop_tracking_process)  --> remove a process from the tracking list

void split_std_string(std::string* str, char delimeter, std::vector<std::string>* sub_strings);

void wait_for_client_to_connect();

void handle_get_complete_data();
void handle_disconnect();
void handle_stop();
void handle_start_tracking_process();
void handle_stop_tracking_process();

// TODO(damian): move these somewhere better, here for now.
static bool   running = true;
static SOCKET client_socket;
static vector<string> message_parts;
static bool need_new_client = true;


int main() {
	 G_state::set_up_on_startup();
	 std::cout << "Done setting up. \n" << std::endl;

	 while (running) {

		 if (need_new_client) {
			 wait_for_client_to_connect();
			 need_new_client = false;
		 }
		
	 	// NOTE: dont forget to maybe extend it id needed, or error if the message is to long or something.
	 	char receive_buffer[512];
	 	int  receive_buffer_len = 512;

		int n_bytes_returned = recv(client_socket, receive_buffer, receive_buffer_len, NULL); // TODO(damian): add a timeout here.
		receive_buffer[n_bytes_returned] = '\0';

		std::cout << "Received a message of " << n_bytes_returned << " bytes." << std::endl;
		std::cout << "Message: " << "'" << receive_buffer << "'" << "\n" << std::endl;

		G_state::update_state();

		// TODO(damian): Would like a switch more here.		

		// NOTE(damian): recv doesnt null terminate the string buffer, 
		//	     so terminating it myself, to then be able to use strcmp.

		// TODO(damian): this might over_flow, handle it, just in case.

		// TODO(damian): create separate handlers for messages. Code would be cleaner. 

		string message_as_std_string = receive_buffer;
		split_std_string(&message_as_std_string, ' ', &message_parts);

		std::cout << "\n\n";
		for (string& part : message_parts) {
			std::cout << part << " --> ";
		}
		std::cout << "\n\n";

		if (message_parts.empty()) assert(false); // TODO(damian): handle better.

		if (message_parts[0] == "get_complete_data")
			handle_get_complete_data();
		else if (message_parts[0] == "start_tracking_process")
			handle_start_tracking_process();
		else if (message_parts[0] == "stop_tracking_process")
			handle_stop_tracking_process();
		else if (message_parts[0] == "disconnect")
			handle_disconnect();
		else if (message_parts[0] == "stop") {
			handle_stop();
		}
		else {
			string message = "Invalid command.";
			int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
			if (send_err_code == SOCKET_ERROR) {
				// TODO: handle
			}

		}

		message_parts.clear();
	 		
	 	// Preserve the new state
	 	std::ofstream data_file("data.json");
			
	 	vector<json> processes_as_jsons;
	 	for (Process_data& process_data : G_state::process_data_vec) {
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
	

	return 0;
}



void split_std_string(std::string* str, char delimeter, std::vector<std::string>* sub_strings) {
	if (str->empty()) return;

	bool have_seen_non_space_char = false;

	size_t start_idx = 0;
	for (int i = 0; i < str->length(); ++i) {
		if (str->c_str()[i] == delimeter) {
			std::string sub_string(str->c_str() + start_idx, i - start_idx);
			sub_strings->push_back(sub_string);
			start_idx = i + 1;
		}
		else {
			have_seen_non_space_char = true;
		}
	}

	if (have_seen_non_space_char) {
		std::string sub_string(str->c_str() + start_idx, str->length() - start_idx);
		sub_strings->push_back(sub_string);
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

void handle_get_complete_data() {
	vector<json> process_as_jsons;
	for (Process_data& data : G_state::process_data_vec) {
		json temp;
		convert_to_json(&temp, &data);
		process_as_jsons.push_back(temp);
	}

	json global_json;
	global_json["processes_to_track"] = process_as_jsons;

	std::string message_as_str = global_json.dump(4);

	int send_err_code = send(client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}

	std::cout << "Message handling: " << message_as_str << "\n" << std::endl;
}

void handle_disconnect() {
	string message = "Client disconnected.";
	
	int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}

	closesocket(client_socket);

	std::cout << "Message handling: " << message << "\n" << std::endl;

	need_new_client = true;
}

void handle_stop() {
	string message = "Stopped traking.";

	int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}

	closesocket(client_socket);

	std::cout << "Message handling: " << message << "\n" << std::endl;
	
	running = false;
}

void handle_start_tracking_process() {
	string path = message_parts[1];

	G_state::Error err_code = G_state::add_process_to_track(&path);

	if (err_code == G_state::Error::ok) return;
	if (err_code == G_state::Error::trying_to_track_the_same_process_more_than_once) {
		string message = "Cant add the track process twice, it is alredy beeing tracked.";
		int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
		if (send_err_code == SOCKET_ERROR) {
			// TODO: handle
		}
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

void handle_stop_tracking_process() {
	string process_to_stop_path = message_parts[1];
	bool   stopped_tracking     = false;

	for (auto it = G_state::process_data_vec.begin(); it != G_state::process_data_vec.end(); ++it) {
		if (it->path == process_to_stop_path) {
			G_state::process_data_vec.erase(it);
			stopped_tracking = true;
			break;
		}
	}

	if (stopped_tracking == false) {
		string message = "The provided process was not beeing tracked, so nothing was removed.";
		int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
		if (send_err_code == SOCKET_ERROR) {
			// TODO: handle
		}
	}
	else {
		string message = "Stopped tracking a process.";
		int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
		if (send_err_code == SOCKET_ERROR) {
			// TODO: handle
		}
	}

}















//else if (match_part_of_string(receive_buffer, n_bytes_returned, "add_process*", 12)) {
				// string buffer_as_str(receive_buffer, n_bytes_returned);
				// int idx_of_delim = buffer_as_str.find("*");

				// string new_exe_name(buffer_as_str.c_str() + idx_of_delim + 1);
				// std::cout << new_exe_name << std::endl;

				// TODO(damian): since process path are not doing anything yet, 
				//				 just manually creating a new process that alredy exists inside data.json

				// Process_data new_process(string("Telegram.exe"), 
				// 							  string("C:\\Users\\Admin\\AppData\\Roaming\\Telegram Desktop\\Telegram.exe"));

				/*Process_data new_process(string("steam.exe"),
											string("C:\\Program Files (x86)\\Steam\\steam.exe"));*/

											//G_state::add_process_to_track(new_process);
										//}

//else {
//								string message = "Message is unsupported. Connection closed.";

//								int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
//								if (send_err_code == SOCKET_ERROR) {
//									// TODO: handle
//								}

//								std::cout << "Message is ussuported: " << message << "\n" << std::endl;

//								closesocket(client_socket);
//								break;

//								}















