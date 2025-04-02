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
#include <AEEngine.h>		// AEVec2
#include "Math.h"
#include "GameData.h"

#undef WINSOCK_VERSION		// fix for macro redefinition
#define WINSOCK_VERSION     2
#define WINSOCK_SUBVERSION  2

#define ERROR_CODE          -1

#define MAX_WORKER_COUNT	10
#define MAX_QUEUE_SLOTS		20

#define DEFAULT_BUFLEN		4096
#define TIMEOUT_MS		    1000
#define TIMEOUT_MS_MAX      10000

#define WIND_SIZE           4
#define SEQ_NUM_MIN         0
#define SEQ_NUM_SPACE       8

#define PACKET_HEADER_SIZE  80


enum class NetworkType 
{
    UNINITIALISED,
    SINGLE_PLAYER,
    CLIENT,
    SERVER
};

enum CommandID
{
    REQ_QUIT = 0x01,
    REQ_CONNECT = 0x02,
    REQ_GAME_START = 0x03,
    RSP_GAME_START = 0x04,
    INPUT_MOVE = 0x10,
    INPUT_STOP = 0x11,
    INPUT_SHOOT = 0x12,
    ENTITY_SNAPSHOT = 0x20,
    ENTITY_SPAWN = 0x21,
    ENTITY_DESTROY = 0x22,
    PLAYER_HIT = 0x30,
    PLAYER_SCORE = 0x31,
    GAME_OVER = 0x32,
    UNKNOWN = 0x50,
    ERROR_PKT = 0x51
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

struct PlayerData;

// Global variables
extern NetworkType networkType;
extern sockaddr_in serverTargetAddress;
extern sockaddr_in clientTargetAddress;
extern uint16_t serverPort;
extern SOCKET udpServerSocket;
extern SOCKET udpClientSocket;

void AttachConsoleWindow();
void FreeConsoleWindow();

int InitialiseNetwork();
int StartServer();
int ConnectToServer();
void Disconnect(SOCKET& socket);

bool SendAck(SOCKET socket, sockaddr_in address, NetworkPacket packet);
void RetransmitPacket();
bool SendPacket(SOCKET socket, sockaddr_in address, NetworkPacket packet, bool is_retransmit);
NetworkPacket ReceivePacket(SOCKET socket, sockaddr_in& address);

void PackPlayerData(NetworkPacket& packet, const PlayerData& player);
void UnpackPlayerData(const NetworkPacket& packet, PlayerData& player);

void PackGameStateData(NetworkPacket& packet, const NetworkGameState& player);
void UnpackGateStateData(const NetworkPacket& packet);

void SendJoinRequest(SOCKET socket, sockaddr_in address);
void HandleJoinRequest(SOCKET socket, sockaddr_in address, NetworkPacket packet);

void SendInput(SOCKET socket, sockaddr_in address);
void HandleClientInput(SOCKET serverUDPSocket, uint16_t clientPortID, std::map<uint16_t, PlayerData>& playersData);
void HandlePlayerInput(uint16_t clientPortID, NetworkPacket& packet, std::map<uint16_t, PlayerData>& playersData);

void SendGameStateStart(SOCKET socket, sockaddr_in address, PlayerData& playerData);
void ReceiveGameStateStart(SOCKET socket, PlayerData& clientData);

void BroadcastGameState(SOCKET socket, std::map<uint16_t, sockaddr_in>& clients);
void ListenForUpdates(SOCKET udpSocket, sockaddr_in serverAddr, PlayerData& clientData);

void GameLoop(std::map<uint16_t, sockaddr_in>& clients);
void Render(NetworkGameState& gameState);

uint16_t GetClientPort();

// Helper functions
uint64_t GetTimeNow();
bool CompareSockaddr(const sockaddr_in& addr1, const sockaddr_in& addr2);
#endif