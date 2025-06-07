#include <thread>
#include <nlohmann/json.hpp>
#include <assert.h>
#include "Functions_file_system.h"
#include "Functions_networking.h"
#include "Functions_win32.h"
#include "Global_state.h"

#include "Lexer.h"
#include "Parser.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

// NOTE(damian): Messages have the following strcture:
//				 <command> <args1> <arg2> ... 

// NOTE(damian): 
// 		Messages that are currently supported:
//			-- (report)   --> get complete data
//			-- (quit)	  --> disconnect the current client
//			-- (shutdown) --> stop the tracking completely
//			-- (track)    --> add a process to be tracked
// 			-- (untrack)  --> remove a process from the tracking list

void split_std_string(std::string* str, char delimeter, std::vector<std::string>* sub_strings);

void wait_for_client_to_connect();

void handle_report  (Report_command*   command);
void handle_quit    (Quit_command*     command);
void handle_shutdown(Shutdown_command* command);
void handle_track   (Track_command*    command);
void handle_untrack (Untrack_command*  command);

// TODO(damian): move these somewhere better, here for now.
static bool   running = true;
static SOCKET client_socket;
static bool need_new_client = true;
// static vector<string> message_parts;



// int main() {
//     Parser parser = parser_init(" pclarity track \"C:\\Users\\Admin\\AppData\\Roaming\\Telegram Desktop\\Telegram.exe\" deadas");
//     pair<Command, bool> result = start_parsing(&parser);

//     if (result.second == true) {
//         std::cout << result.first.track.path_token.lexeme << std::endl;
//     }
//     else {
//         std::cout << "invalid" << std::endl;
//     }

//     return 0;
// }



int main() {

 	G_state::set_up_on_startup();
 	std::cout << "Done setting up. \n" << std::endl;

 	while (running) {

 		if (need_new_client) {
 			wait_for_client_to_connect();
 			need_new_client = false;
 		}
		
 		// NOTE: dont forget to maybe extend it if needed, or error if the message is to long or something.
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

 		// string message_as_std_string = receive_buffer;
 		// split_std_string(&message_as_std_string, ' ', &message_parts);

        // Parsing the request.
        Parser parser = parser_init(receive_buffer);
        pair<Command, bool> result = start_parsing(&parser);

        if (result.second == false)  {
            string message = "Invalid command.";
 			int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
 			if (send_err_code == SOCKET_ERROR) {
 				// TODO: handle
 			}
        }
        else {
            switch (result.first.type) {
                case Command_type::report: {
                    handle_report(&result.first.report);
                } break;
                
                case Command_type::quit: {
                    handle_quit(&result.first.quit);
                } break;

                case Command_type::shutdown: {
                    handle_shutdown(&result.first.shutdown);
                } break;

                case Command_type::track: {
                    handle_track(&result.first.track);
                } break;

                case Command_type::untrack: {
                    handle_untrack(&result.first.untrack);
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

	return 0;
}

// TODO(damian): handle out of bounds for message_parts. 
// TODO(damian): add something that will represent the expectations for the message command, then it will be responcible for message validity checks.

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

void handle_report(Report_command* command) {
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

void handle_quit(Quit_command* command) {
	string message = "Client disconnected.";
	
	int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}

	closesocket(client_socket);

	std::cout << "Message handling: " << message << "\n" << std::endl;

	need_new_client = true;
}

void handle_shutdown(Shutdown_command* command) {
	string message = "Stopped traking.";

	int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
	if (send_err_code == SOCKET_ERROR) {
		// TODO: handle
	}

	closesocket(client_socket);

	std::cout << "Message handling: " << message << "\n" << std::endl;
	
	running = false;
}

void handle_track(Track_command* command) {
    const char* path_start  = command->path_token.lexeme + 1; // Skipping the first " 
	size_t      path_length = command->path_token.length - 2; // Skipping the first " and the last "

    string path(path_start, path_length);

	G_state::Error err_code = G_state::add_process_to_track(&path);

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

void handle_untrack(Untrack_command* command) {
	const char* path_start  = command->path_token.lexeme + 1; // Skipping the first " 
	size_t      path_length = command->path_token.length - 2; // Skipping the first " and the last "

    string path(path_start, path_length);

	G_state::Error err_code = G_state::remove_process_from_track(&path);

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
















