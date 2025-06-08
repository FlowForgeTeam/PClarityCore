#pragma once

#include <winsock2.h>
#include <iostream>
#include <ws2tcpip.h>
#include <utility>
#pragma comment(lib, "ws2_32") // Linking the dll
#include "Global_state.h"

using std::pair;

// enum class Network_error {
    // ok,
// };

pair<SOCKET, G_state::Error> initialise_tcp_connection_with_client();

