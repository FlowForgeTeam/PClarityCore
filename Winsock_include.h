#pragma once

#ifndef WINSOCK_WRAPPER_INCLUDED
#define WINSOCK_WRAPPER_INCLUDED

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32") // Linking the dll

#endif // WINSOCK_WRAPPER_INCLUDED







