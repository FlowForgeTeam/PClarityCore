#include "Functions_networking.h"

// TODO: dont knwo how to handle different types of errors here, there are too many of them shits.

pair<SOCKET, G_state::Error> initialise_tcp_connection_with_client() {
	// Getting data about windows socket implementation
	WSADATA wsa_data;
	
	// Using MAKEWORD(2,2) since 2.2 is the current spec version for Windows
	// NOTE(damian): To better enderstand what this is, read https://stackoverflow.com/questions/4991967/how-does-wsastartup-function-initiates-use-of-the-winsock-dll
	int wsa_err = WSAStartup(MAKEWORD(2, 2), &wsa_data); 
	if (wsa_err != 0) 
		return pair(SOCKET{0}, G_state::Error::WSASTARTUP_failed);

	// Getting local host address info
	addrinfo host_address_info = {};
	addrinfo* host_address_info_p = &host_address_info;
	int add_info_err = getaddrinfo(NULL, "12345", NULL, &host_address_info_p);
	
	if (add_info_err != 0) return pair(SOCKET{0}, G_state::Error::getaddrinfo_failed);

	// Creating a socket for the server to listen for client connections
	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(12345);

	int bind_err = bind(server_socket, (struct sockaddr*)&server, sizeof(server));
	if (bind_err != 0) return pair(SOCKET{0}, G_state::Error::bind_failed);

	int listen_err = listen(server_socket, 1);
	if (listen_err != 0) return pair(SOCKET{0}, G_state::Error::listen_failed);

	freeaddrinfo(host_address_info_p);

	// Accepting a client socket
	SOCKET client_socket = accept(server_socket, NULL, NULL);
	if (client_socket == INVALID_SOCKET) return pair(SOCKET{0}, G_state::Error::accept_failed); 

	// TODO: dont know why this is here
	int close_err = closesocket(server_socket);
	if (close_err == SOCKET_ERROR) return pair(SOCKET{0}, G_state::Error::closesocket_failed);

	return pair(client_socket, G_state::Error::ok);
}
