#include <thread>
#include <nlohmann/json.hpp>
#include <assert.h>
#include "Functions_file_system.h"
#include "Functions_networking.h"
#include "Functions_win32.h"
#include "Global_state.h"

// TODO:
// 		1. Prefetch all the possible EXEs on the pc.
//		2. Add a command to be send by the clint to retrive all possible EXEs.

#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

// namespace Server_message {
// 	enum Type {
// 		Get_data,
// 	};
// }

// TEMPORARY
bool c_str_equals(const char* str1, int len1, const char* str2, int len2) {
	if (len1 != len2) return false;

	for (int i = 0; i < len1; ++i) {
		if (str1[i] != str2[i])	return false;
	}

	return true;
}

int main() {

	G_state::set_up_on_startup();

	std::vector<std::string> keys;

	for (auto& el : G_state::data_as_json.items()) {
		std::cout << el.key() << ": " << el.value() << std::endl;
	}

	std::cout << "Waiting for connection with a client. \n";
	SOCKET client_socket = initialise_tcp_connection_with_client();
	std::cout << "Client connected. \n\n";

	// NOTE: dont forget to maybe extend it id needed, or error if the message is to long or something.
	char receive_buffer[512];
	int  receive_buffer_len = 512;

	auto start = std::chrono::steady_clock::now();
	auto was_active_for = std::chrono::seconds(0);
	bool was_active = false;

	std::cout << G_state::data_as_json << std::endl;

	// NOTE: since only tracking telegram for this demo, hard code the was_active_for value here
	if (G_state::data_as_json.contains("time_spent_active")) {
		was_active_for = std::chrono::seconds(G_state::data_as_json["time_spent_active"]);
	}
	else {
		was_active_for = std::chrono::seconds(0);
	}

	while (true) {
		// This is the requst message in bytes
		receive_buffer[0] = '\0';
		int n_bytes_returned = recv(client_socket, receive_buffer, receive_buffer_len, NULL);

		// Handle messages here
		if (n_bytes_returned > 0) {

			std::cout << receive_buffer << std::endl;

			// NOTE: dont use strcmp, since the message might not end with \0

			// TODO: Would like a switch more here			
			if (c_str_equals(receive_buffer, n_bytes_returned, "get_data", 8)) {
				// Getting info about if Telegram.exe is active
				bool is_active = is_process_active_by_name(L"Telegram.exe");
				if (is_active == true && was_active == false) {
					start = std::chrono::steady_clock::now();
				}
				else if (is_active == false && was_active == true) {
					was_active_for += std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::steady_clock::now() - start
					);
					start = std::chrono::steady_clock::now();

				}
				else if (is_active == true && was_active == true) {
					was_active_for += std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::steady_clock::now() - start
					);
					start = std::chrono::steady_clock::now();

				}
				was_active = is_active;

				std::cout << "\n was_active_for.count() --> " << was_active_for.count() << "\n\n";

				// NOTE: json represent telegram info only, for now.

				G_state::data_as_json["is_active"] = is_active;
				G_state::data_as_json["time_spent_active"] = was_active_for.count();

				std::string message_as_str = G_state::data_as_json.dump();
				int sent_err_code = send(client_socket, message_as_str.c_str(), message_as_str.size(), NULL);

				if (sent_err_code == SOCKET_ERROR) {
					std::cout << "Error when sending data to client. \n";
				}
				std::cout << "Bytes sent. \n";
			}
			else if (c_str_equals(receive_buffer, n_bytes_returned, "stop", 4)) {
				closesocket(client_socket);

				std::cout << "Connection with current client closed. \n";
				break;
			}
			else {
				assert(false);
			}
		}
	}

	// Preserve the new state
	std::ofstream new_file("data.json");
	new_file << G_state::data_as_json.dump(4) << std::endl;
	new_file.close();

	//// Closing the socket
	//int err_code = shutdown(client_socket, SD_SEND);
	//if (err_code == SOCKET_ERROR) {
	//	printf("shutdown failed with error: %d\n", WSAGetLastError());
	//	closesocket(client_socket);
	//	WSACleanup();
	//	return 1;
	//}

	//// Cleanup
	//closesocket(client_socket);


	return 0;
}





















