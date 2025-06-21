#pragma once

#include <iostream>
#include <utility>
#include "Winsock_include.h"
#include "Global_state.h"

using std::pair;

pair<SOCKET, G_state::Error> initialise_tcp_connection_with_client();

