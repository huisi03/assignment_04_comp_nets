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
#include <unordered_map>    // unordered map
#include "../Math.h"           // Math

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

enum InputKey
{
    NONE,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    SPACE
};

enum Datatype
{
    PLAYER,
    BULLET,
    ASTEROID,
    SCORE
};

struct NetworkPacket
{
    NetworkPacket()
    {
        memset(data, 0, DEFAULT_BUFLEN);  // Ensures data is fully null-terminated
    }

    uint16_t packetID;
    uint16_t sourcePortNumber;
    uint16_t destinationPortNumber;
    char data[DEFAULT_BUFLEN];
};

struct PlayerData
{
    float posX, posY;      // Position
    float scaleX, scaleY;  // Scale
    float velX, velY;      // Velocity (defaults to 0)
    float rotation;        // Rotation (defaults to 0)

    // Default constructor (needed for std::map)
    PlayerData() : posX(0), posY(0), scaleX(0), scaleY(0), velX(0), velY(0), rotation(0) {}

    // Constructor: Initializes pos and scale, defaults velocity and rotation to 0
    PlayerData(float x, float y, float sx, float sy)
        : posX(x), posY(y), scaleX(sx), scaleY(sy), velX(0.0f), velY(0.0f), rotation(0.0f) {}
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

void PackPlayerData(NetworkPacket& packet, const PlayerData& player);
void UnpackPlayerData(const NetworkPacket& packet, PlayerData& player);

void SendJoinRequest(SOCKET socket, sockaddr_in address);
void HandleJoinRequest(SOCKET socket, sockaddr_in address, NetworkPacket packet);

void SendInput(SOCKET socket, sockaddr_in address);
void HandleClientInput(SOCKET serverUDPSocket, uint16_t clientPortID, std::map<uint16_t, PlayerData>& playersData);
void HandlePlayerInput(uint16_t clientPortID, NetworkPacket packet, std::map<uint16_t, PlayerData>& playersData);

void SendGameStateStart(SOCKET socket, sockaddr_in address, PlayerData& playerData);
void ReceiveGameStateStart(SOCKET socket, PlayerData& clientData);

void BroadcastGameState(SOCKET socket, std::map<uint16_t, sockaddr_in>& clients, std::map<uint16_t, PlayerData>& playersData);
void ListenForUpdates(SOCKET udpSocket, sockaddr_in serverAddr, PlayerData& clientData);

uint16_t GetClientPort();

#endif