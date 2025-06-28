#include "Functions_networking.h"

using G_state::Error_type;
using G_state::Error;

pair<SOCKET, G_state::Error> initialise_tcp_connection_with_client() {
	// Getting data about windows socket implementation
	// Using MAKEWORD(2,2) since 2.2 is the current spec version for Windows
	// NOTE(damian): To better enderstand what this is, read https://stackoverflow.com/questions/4991967/how-does-wsastartup-function-initiates-use-of-the-winsock-dll
	WSADATA wsa_data;
	int err_code = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (err_code != 0) {
		return pair(SOCKET{0}, Error(Error_type::tcp_initialisation_failed, "WSA failed, wsa_err_code: " + err_code));
	}

	// Creating a socket for the server to listen for client connections
	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == INVALID_SOCKET) {
		WSACleanup();
		return pair(SOCKET{0}, Error(Error_type::tcp_initialisation_failed, "Failed to create socket, wsa_err_code: " + WSAGetLastError()));
	}

	sockaddr_in server     = {0};
	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port        = htons(12345);

	int bind_err = bind(server_socket, (struct sockaddr*)&server, sizeof(server));
	if (bind_err != 0) {
		closesocket(server_socket);
		WSACleanup();
		return pair(SOCKET{0}, Error(Error_type::tcp_initialisation_failed, "Failed when creating a server socker, wsa_err_code: " + WSAGetLastError()));
	}

	int listen_err = listen(server_socket, 1);
	if (listen_err != 0) {
		closesocket(server_socket);
		WSACleanup();
		return pair(SOCKET{0}, Error(Error_type::tcp_initialisation_failed, "Failed when listening for an incoming connection, wsa_err_code: " + WSAGetLastError()));
	}

	// Accepting a client socket
	SOCKET client_socket = accept(server_socket, NULL, NULL);
	if (client_socket == INVALID_SOCKET) {
		closesocket(server_socket);
		WSACleanup();
		return pair(SOCKET{0}, Error(Error_type::tcp_initialisation_failed, "Failed when accepting a client connection, wsa_err_code: " + WSAGetLastError()));
	}

	// Closing the server socket since we now have client socket. All the comunication will be done throught it
	int close_err = closesocket(server_socket);
	if (close_err == SOCKET_ERROR) { // TODO(damian): we still can use client socket, no real need here to stop everything. THINK ABOUT THIS.
		closesocket(client_socket);
		WSACleanup();
		return pair(SOCKET{0}, Error(Error_type::tcp_initialisation_failed, "Failed when closing a server socket, wsa_err_code: " + WSAGetLastError()));
	}

	return pair(client_socket, Error(Error_type::ok));
}


