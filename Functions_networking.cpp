#include "Functions_networking.h"

// TODO: dont knwo how to handle different types of errors here, there are too many of them shits.

SOCKET initialise_tcp_connection_with_client() {
	// Getting data about windows socket implementation
	WSADATA wsa_data;
	
	// Using MAKEWORD(2,2) since 2.2 is the current spec version for Windows
	// NOTE(damian): To better enderstand what this is, read https://stackoverflow.com/questions/4991967/how-does-wsastartup-function-initiates-use-of-the-winsock-dll
	int wsa_err = WSAStartup(MAKEWORD(2, 2), &wsa_data); 
	if (wsa_err != 0) {
		// TODO: handle this, also there are a bunch of different error that might occur, read docs.
	}

	// Getting local host address info
	addrinfo host_address_info = {};
	addrinfo* host_address_info_p = &host_address_info;
	int add_info_err = getaddrinfo(NULL, "12345", NULL, &host_address_info_p);
	if (add_info_err != 0) {
		// TODO: handle this, also there are a bunch of different error that might occur, read docs.
	}

	// Creating a socket for the server to listen for client connections
	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(12345);

	bind(server_socket, (struct sockaddr*)&server, sizeof(server));
	listen(server_socket, 1);

	freeaddrinfo(host_address_info_p);

	int listen_err = listen(server_socket, SOMAXCONN);
	if (listen_err != 0) {
		// TODO: read docs for better error explanation, 
		//		 close_err is not jsut 0 or 1 here. Use WASGetLastError to retrive error. 
	}

	// Accepting a client socket
	SOCKET client_socket = accept(server_socket, NULL, NULL);
	if (client_socket == INVALID_SOCKET) {
		// TODO: read docs for better error explanation, 
		//		 close_err is not jsut 0 or 1 here. Use WASGetLastError to retrive error. 
	}

	// TODO: dont know why this is here
	int close_err = closesocket(server_socket);
	if (close_err == SOCKET_ERROR) {
		// TODO: read docs for better error explanation, 
		//		 close_err is not jsut 0 or 1 here. Use WASGetLastError to retrive error. 
	}

	return client_socket;
}
