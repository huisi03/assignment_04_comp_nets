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
#include <vector>           // vector

#undef WINSOCK_VERSION		// fix for macro redefinition
#define WINSOCK_VERSION     2
#define WINSOCK_SUBVERSION  2

#define ERROR_CODE          -1

#define MAX_WORKER_COUNT	10
#define MAX_QUEUE_SLOTS		20

#define DEFAULT_BUFLEN		4096
#define TIMEOUT_MS		    1000
#define TIMEOUT_MS_MAX      10000

#define SENDER_WIND_SIZE    4
#define RECV_WIND_SIZE      4
#define SEQ_NUM_MIN         0
#define SEQ_NUM_MAX         7

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
    RSP_CONNECT = 0x03,
    REQ_GAME_START = 0x04,
    RSP_GAME_START = 0x05,
    INPUT_MOVE = 0x10,
    INPUT_STOP = 0x11,
    INPUT_SHOOT = 0x12,
    ENTITY_SNAPSHOT = 0x20,
    ENTITY_SPAWN = 0x21,
    ENTITY_DESTROY = 0x22,
    PLAYER_HIT = 0x30,
    PLAYER_SCORE = 0x31,
    GAME_OVER = 0x32,
    UNKNOWN = 0x50
};

struct NetworkPacket
{
    uint8_t commandID = UNKNOWN;
    uint8_t flags;
    uint32_t seqNumber;
    uint32_t dataLength = 0;
    char data[DEFAULT_BUFLEN]{ 0 };
};

// Global variables
extern NetworkType networkType;
extern uint32_t nextSeqNum;
extern uint32_t sendBase;
extern uint32_t recvBase;
extern std::map<uint32_t, NetworkPacket> sendBuffer;               // Packets awaiting ACK
extern std::map<uint32_t, NetworkPacket> recvBuffer;               // Stores received packets
extern std::map<uint32_t, uint64_t> timers;                        // Timeout tracking
extern std::map<uint32_t, sockaddr_in> clientTargetAddresses;      // used for NetworkType::SERVER to keep track of connect clients
extern sockaddr_in serverTargetAddress;                            // used for NetworkType::CLIENT
extern uint16_t serverPort;                                        // used for NetworkType::CLIENT
extern uint16_t clientPort;                                        // used for NetworkType::SERVER
extern SOCKET udpClientSocket;                                     // used for NetworkType::CLIENT
extern SOCKET udpServerSocket;                                     // used for NetworkType::SERVER

extern const std::string configFileRelativePath;
extern const std::string configFileServerIp;
extern const std::string configFileServerPort;

void AttachConsoleWindow();
void FreeConsoleWindow();

int InitialiseNetwork();
int StartServer();
int ConnectToServer();
void Disconnect();

void AwaitAck();
void SendPacket(SOCKET socket, sockaddr_in address, NetworkPacket packet);
NetworkPacket ReceivePacket(SOCKET socket, sockaddr_in& address);
bool CompareSockaddr(const sockaddr_in& addr1, const sockaddr_in& addr2);

void SendJoinRequest(SOCKET socket, sockaddr_in address);
void HandleJoinRequest(SOCKET socket, sockaddr_in address, NetworkPacket packet);

void SendInput(SOCKET socket, sockaddr_in address);
void HandlePlayerInput(SOCKET socket, sockaddr_in address, NetworkPacket packet);

void SendGameStateStart(SOCKET socket, sockaddr_in address);
void ReceiveGameStateStart(SOCKET socket);

uint64_t GetTimeNow();

#endif