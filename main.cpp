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

// TEMPORARY
//bool c_str_equals(const char* str1, int len1, const char* str2, int len2) {
//	if (len1 != len2) return false;
//
//	for (int i = 0; i < len1; ++i) {
//		if (str1[i] != str2[i])	return false;
//	}
//
//	return true;
//}

//bool match_part_of_string(const char* str, int len_str, const char* str_to_match, int len_str_to_match) {
//	for (int i = 0; i < len_str; ++i)
//		std::cout << str[i];
//	std::cout << "\n";
//
//	for (int i = 0; i < len_str_to_match; ++i)
//		std::cout << str_to_match[i];
//	std::cout << "\n";
//	
//	for (int i = 0; i < len_str_to_match; ++i) {
//		if (str[i] != str_to_match[i])
//			return false;
//	}
//	return true;
//}

int main() {
	G_state::set_up_on_startup();

	/*for (Process_data& data: G_state::process_data_vec) {
		std::cout << "name: " << data.name << ", time: " << data.time_spent.count() << " sec" << std::endl;
	}*/
	
	std::cout << "Waiting for connection with a client. \n";
	SOCKET client_socket;
	pair<SOCKET, Network_error> result = initialise_tcp_connection_with_client();
	if (result.second == Network_error::ok) client_socket = result.first;
	else assert(false);
	std::cout << "Client connected. \n\n";
	
	// NOTE: dont forget to maybe extend it id needed, or error if the message is to long or something.
	char receive_buffer[512];
	int  receive_buffer_len = 512;

	while (true) {
		// This is the requst message in bytes
		// TODO(damian): this has unknown behaviour is the sended tries to send "", handle this.
		int n_bytes_returned = recv(client_socket, receive_buffer, receive_buffer_len, NULL);
		
		G_state::update_state();

		// Handle messages here
		if (n_bytes_returned > 0) {
			// TODO(damian): Would like a switch more here.		

			// NOTE(damian): recv doesnt null terminate the string buffer, 
			//	     so terminating it myself, to then be able to use strcmp.

			// TODO(damian): this might over_flow, handle it, just in case.
			receive_buffer[n_bytes_returned] = '\0';

			std::cout << "TESTING BUFFER --> " << receive_buffer << std::endl;

			if (strcmp(receive_buffer, "get_data") == 0) {
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

				std::cout << "Bytes sent." << std::endl;;
				std::cout << message_as_str << std::endl;
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
			else if (strcmp(receive_buffer, "stop") == 0) {
				string message = "stopped the connection.";
				
				int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
				if (send_err_code == SOCKET_ERROR) {
					// TODO: handle
				}

				closesocket(client_socket);

				std::cout << "Connection with current client closed. \n";

				break;
			}
			else {
				string message = "message is unsupported. Connection closed.";

				int send_err_code = send(client_socket, message.c_str(), message.length(), NULL);
				if (send_err_code == SOCKET_ERROR) {
					// TODO: handle
				}

				std::cout << "Message is ussuported: " << receive_buffer << std::endl;
				
				closesocket(client_socket);
				break;

			}

		}

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

	}

	return 0;
}






// int main() {
// 	int err_code;
// 	DWORD process_ids_buffer[512];
// 	int bytes_returned = get_all_active_processe_ids(process_ids_buffer, 512, &err_code);

// 	WCHAR process_name_buffer[512];
// 	WCHAR process_path_buffer[512];

// 	for (int i = 0; i < (bytes_returned / sizeof(DWORD)); ++i) {
// 		std::cout << i << ": " << process_ids_buffer[i] << std::endl;

// 		err_code = get_process_name(process_ids_buffer[i], process_name_buffer, 512);
// 		if (err_code == 0) {
// 			std::wcout << L"\tName: " << process_name_buffer << std::endl;
// 		}

// 		err_code = get_process_path(process_ids_buffer[i], process_path_buffer, 512);
// 		if (err_code == 0) {
// 			std::wcout << L"\tPath: " << process_path_buffer << std::endl;
// 		}
// 	}

// 	return 0;
// }


















