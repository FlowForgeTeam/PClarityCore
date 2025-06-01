#pragma once

#include <winsock2.h>
#include <iostream>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32") // Linking the dll

SOCKET initialise_tcp_connection_with_client();

