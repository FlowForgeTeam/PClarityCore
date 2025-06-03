#pragma once

#include <winsock2.h>
#include <iostream>
#include <ws2tcpip.h>
#include <utility>
#pragma comment(lib, "ws2_32") // Linking the dll

using std::pair;

enum class Network_error {
    ok,
};

pair<SOCKET, Network_error> initialise_tcp_connection_with_client();

