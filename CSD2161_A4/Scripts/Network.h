#ifndef NETWORK
#define NETWORK // header guards

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Windows.h"		// Entire Win32 API...
// #include "winsock2.h"	// ...or Winsock alone
#include "ws2tcpip.h"		// getaddrinfo()

// Tell the Visual Studio linker to include the following library in linking.
// Alternatively, we could add this file to the linker command-line parameters,
// but including it in the source code simplifies the configuration.
#pragma comment(lib, "ws2_32.lib")

#include <iostream>			// cout, cerr
#include <string>			// string
#include <optional>			// optional
#include <filesystem>		// file system
#include <fstream>			// file stream
#include <map>	            // map
#include <mutex>			// mutex

#undef WINSOCK_VERSION		// fix for macro redefinition
#define WINSOCK_VERSION     2
#define WINSOCK_SUBVERSION  2

#define ERROR_CODE          -1

#define MAX_WORKER_COUNT	10
#define MAX_QUEUE_SLOTS		20

#define DEFAULT_BUFLEN		4096
#define TIMEOUT_MS		    1000

enum class NetworkType 
{
    UNINITIALISED,
    SINGLE_PLAYER,
    CLIENT,
    SERVER
};

enum PacketID 
{
    JOIN_REQUEST,
    REQUEST_ACCEPTED,
    GAME_INPUT,
    GAME_STATE_START,
    GAME_STATE_UPDATE
};

struct NetworkPacket
{
    uint16_t packetID;
    uint16_t sourcePortNumber;
    uint16_t destinationPortNumber;
    char data[DEFAULT_BUFLEN];
};

// Global variables
extern NetworkType networkType;
extern sockaddr_in targetAddress;
extern uint16_t port;
extern SOCKET udpSocket;

void AttachConsoleWindow();
void FreeConsoleWindow();

int InitialiseNetwork();
int StartServer();
int ConnectToServer();
void Disconnect();

void SendPacket(SOCKET socket, sockaddr_in address, NetworkPacket packet);
NetworkPacket ReceivePacket(SOCKET socket, sockaddr_in& address);

void SendJoinRequest(SOCKET socket, sockaddr_in address);
void HandleJoinRequest(SOCKET socket, sockaddr_in address, NetworkPacket packet);

void SendInput(SOCKET socket, sockaddr_in address);
void HandlePlayerInput(SOCKET socket, sockaddr_in address, NetworkPacket packet);

void SendGameStateStart(SOCKET socket, sockaddr_in address);
void ReceiveGameStateStart(SOCKET socket);

#endif