#include "Network.h"
#include "Main.h"		// main headers

// Define
NetworkType networkType = NetworkType::UNINITIALISED;
sockaddr_in serverTargetAddress{};                              // used for NetworkType::CLIENT
sockaddr_in clientTargetAddress{};								// used for NetworkType::SERVER
uint16_t serverPort;
uint16_t clientPort;
SOCKET udpClientSocket = INVALID_SOCKET;                        // used for NetworkType::CLIENT
SOCKET udpServerSocket = INVALID_SOCKET;                        // used for NetworkType::SERVER
std::mutex packetMutex;
std::mutex playerDataMutex;

uint32_t nextSeqNum = SEQ_NUM_MIN;
uint32_t sendBase = SEQ_NUM_MIN;
uint32_t recvBase = SEQ_NUM_MIN;
std::map<uint32_t, NetworkPacket> sendBuffer{};                 // Packets awaiting ACK
std::map<uint32_t, NetworkPacket> recvBuffer{};                 // Stores received packets
std::set<uint32_t> ackedPackets;                                // Tracks received ACKs
std::map<uint32_t, uint64_t> timers{};                          // Timeout tracking of un-ACK packets

const std::string configFileRelativePath = "Resources/Configuration.txt";
const std::string configFileServerIp = "serverIp";
const std::string configFileServerPort = "serverUdpPort";


void AttachConsoleWindow()
{
	AllocConsole();
	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONIN$", "r", stdin);
}

void FreeConsoleWindow()
{
	FreeConsole();
}

int InitialiseNetwork()
{
	std::string networkTypeString;
	std::cout << "Network Type (S for Server | C for Client | Default for Single Player): ";
	std::getline(std::cin, networkTypeString);

	if (networkTypeString == "S")
	{
		networkType = NetworkType::SERVER;
		std::cout << "Initialising as Server..." << std::endl;
		return StartServer();
	}
	else if (networkTypeString == "C") 
	{
		networkType = NetworkType::CLIENT;
		std::cout << "Initialising as Client..." << std::endl;
		return ConnectToServer();
	}
	else
	{
		networkType = NetworkType::SINGLE_PLAYER;
		std::cout << "Initialising as Single Player..." << std::endl;
		return 0;
	}
	
	return ERROR_CODE;
}

int StartServer()
{
    // Get UDP Port Number
    std::string portString{};
    std::ifstream config_file(configFileRelativePath);
    if (config_file.is_open()) {
        std::string buffer{};
        while (std::getline(config_file, buffer)) {
            std::string::size_type findIndex = buffer.find(configFileServerPort);
            if (findIndex != std::string::npos) {
                portString = buffer.substr(findIndex + configFileServerPort.size() + 1);
                serverPort = static_cast<uint16_t>(std::stoi(portString));
            }
        }
    }

    if (portString.empty()) {
        return ERROR_CODE;
    }

    // Setup WSA data
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(WINSOCK_VERSION, WINSOCK_SUBVERSION), &wsaData) != 0)
    {
        std::cerr << "WSAStartup() failed." << std::endl;
        return ERROR_CODE;
    }

    // Setup UDP hints
    addrinfo udpHints{};
    SecureZeroMemory(&udpHints, sizeof(udpHints));
    udpHints.ai_family = AF_INET;        // IPv4
    udpHints.ai_socktype = SOCK_DGRAM;   // Datagram (unreliable)
    udpHints.ai_protocol = IPPROTO_UDP;  // UDP

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR)
    {
        std::cerr << "gethostname() failed. Error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return ERROR_CODE;
    }

    addrinfo* udpInfo = nullptr;
    if (getaddrinfo(hostname, portString.c_str(), &udpHints, &udpInfo) != 0)
    {
        std::cerr << "getaddrinfo() failed. Error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return ERROR_CODE;
    }

    // Create a UDP socket
    udpServerSocket = socket(udpInfo->ai_family, udpInfo->ai_socktype, udpInfo->ai_protocol);
    if (udpServerSocket == INVALID_SOCKET)
    {
        std::cerr << "socket() failed." << std::endl;
        freeaddrinfo(udpInfo);
        WSACleanup();
        return ERROR_CODE;
    }

    // Create a sockaddr_in for binding
    sockaddr_in bindAddress{};
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = htons(serverPort);
    bindAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(udpServerSocket, (sockaddr*)&bindAddress, sizeof(bindAddress)) == SOCKET_ERROR)
    {
        std::cerr << "bind() failed." << std::endl;
        closesocket(udpServerSocket);
        freeaddrinfo(udpInfo);
        WSACleanup();
        return ERROR_CODE;
    }

    // Print server IP address and serverPort number
    char serverIPAddress[DEFAULT_BUFLEN];
    struct sockaddr_in* address = reinterpret_cast<struct sockaddr_in*> (udpInfo->ai_addr);
    inet_ntop(AF_INET, &(address->sin_addr), serverIPAddress, INET_ADDRSTRLEN);
    getnameinfo(udpInfo->ai_addr, static_cast <socklen_t>(udpInfo->ai_addrlen), serverIPAddress, sizeof(serverIPAddress), nullptr, 0, NI_NUMERICHOST);

    std::cout << std::endl;
    std::cout << "Server has been established" << std::endl;
    std::cout << "Server IP Address: " << serverIPAddress << std::endl;
    std::cout << "Server UDP Port Number: " << serverPort << std::endl;
    std::cout << std::endl;

    freeaddrinfo(udpInfo);

    return 0;
}

int ConnectToServer()
{
    // Get IP Address & UDP Server Port Number
    std::string serverIPAddress{};
    std::string serverPortString{};
    std::ifstream config_file(configFileRelativePath);
    if (config_file.is_open()) {
        std::string buffer{};
        while (std::getline(config_file, buffer)) {
            std::string::size_type findIndex = buffer.find(configFileServerIp);
            if (findIndex != std::string::npos) {
                serverIPAddress = buffer.substr(findIndex + configFileServerIp.size() + 1);
            }
            findIndex = buffer.find(configFileServerPort);
            if (findIndex != std::string::npos) {
                serverPortString = buffer.substr(findIndex + configFileServerPort.size() + 1);
                serverPort = static_cast<uint16_t>(std::stoi(serverPortString));
            }
        }
    }
    if (serverIPAddress.empty() || serverPortString.empty()) {
        return ERROR_CODE;
    }

    // Get UDP Client Port Number
    std::string portString;
    std::cout << "Client UDP Port Number: ";
    std::getline(std::cin, portString);
    clientPort = static_cast<uint16_t>(std::stoi(portString));

    // Setup WSA data
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(WINSOCK_VERSION, WINSOCK_SUBVERSION), &wsaData) != 0)
    {
        std::cerr << "WSAStartup() failed." << std::endl;
        return ERROR_CODE;
    }

    // Setup UDP hints
    addrinfo udpHints{};
    SecureZeroMemory(&udpHints, sizeof(udpHints));
    udpHints.ai_family = AF_INET;        // IPv4
    udpHints.ai_socktype = SOCK_DGRAM;   // Datagram (unreliable)
    udpHints.ai_protocol = IPPROTO_UDP;  // UDP

    addrinfo* udpInfo = nullptr;
    if (getaddrinfo(serverIPAddress.c_str(), serverPortString.c_str(), &udpHints, &udpInfo) != 0)
    {
        std::cerr << "getaddrinfo() failed. Error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return ERROR_CODE;
    }

    // Create a UDP Socket
    udpClientSocket = socket(udpInfo->ai_family, udpInfo->ai_socktype, udpInfo->ai_protocol);
    if (udpClientSocket == INVALID_SOCKET)
    {
        std::cerr << "socket() failed." << std::endl;
        freeaddrinfo(udpInfo);
        WSACleanup();
        return ERROR_CODE;
    }

    // Bind the socket to the client port for receiving data
    sockaddr_in clientAddr{};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;  // Accept packets from any interface
    clientAddr.sin_port = htons(clientPort);

    if (bind(udpClientSocket, (sockaddr*)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR)
    {
        std::cerr << "bind() failed. Error: " << WSAGetLastError() << std::endl;
        closesocket(udpClientSocket);
        freeaddrinfo(udpInfo);
        WSACleanup();
        return ERROR_CODE;
    }

    // Store target address
    serverTargetAddress = *(reinterpret_cast<sockaddr_in*>(udpInfo->ai_addr));

    // Print server IP address and port number
    std::cout << std::endl;
    std::cout << "Client has been established" << std::endl;
    std::cout << "Server IP Address: " << serverIPAddress << std::endl;
    std::cout << "Server UDP Port Number: " << serverPort << std::endl;
    std::cout << "Client UDP Port Number: " << clientPort << std::endl;
    std::cout << std::endl;

    freeaddrinfo(udpInfo);

    // Send a REQ_CONNECT
    std::cout << std::endl;
    std::cout << "Client is sending a request connection to server...";
    std::cout << std::endl;

    NetworkPacket requestConnectionSegment{};
    requestConnectionSegment.sourcePortNumber = clientPort;
    requestConnectionSegment.destinationPortNumber = serverPort;
    requestConnectionSegment.packetID = REQ_CONNECT;
    requestConnectionSegment.flags = 0;

    if (SendPacket(udpClientSocket, serverTargetAddress, requestConnectionSegment)) {
        // Await for ack of sent segment
        while (true) {
            sockaddr_in address{};
            NetworkPacket packet = ReceivePacket(udpClientSocket, address);
            if (packet.packetID != REQ_CONNECT) {
                RetransmitPacket();
            }
            else {
                break;
            }
        }

    }
    else {
        return ERROR_CODE;
    }

    return 0;
}

void Disconnect(SOCKET& socket)
{
	shutdown(socket, SD_SEND);
	closesocket(socket);
	WSACleanup();
}

bool SendPacket(SOCKET socket, sockaddr_in address, NetworkPacket packet, bool is_retransmit)
{
    if (is_retransmit) {

        int sentBytes = sendto(socket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&address, sizeof(address));

        if (sentBytes == SOCKET_ERROR) {

            std::cerr << "Failed to send game data. Error: " << WSAGetLastError() << std::endl;

            return false;
        }

    } else if ((nextSeqNum + 1 % SEQ_NUM_SPACE != (sendBase + WIND_SIZE) % SEQ_NUM_SPACE) || networkType == NetworkType::SERVER) {

        std::cout << std::endl;
        std::cout << "Sending packet of sequence number: " << nextSeqNum << std::endl;
        std::cout << "Packet id is " << std::to_string(packet.packetID) << std::endl;
        std::cout << "Flag is " << std::to_string(packet.flags) << std::endl;
        std::cout << std::endl;

        packet.seqNumber = nextSeqNum;

        int sentBytes = sendto(socket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&address, sizeof(address));

        if (sentBytes == SOCKET_ERROR) {

            std::cerr << "Failed to send game data. Error: " << WSAGetLastError() << std::endl;

            return false;

        } else {

            nextSeqNum = (nextSeqNum + 1) % SEQ_NUM_SPACE;

            if (networkType != NetworkType::SERVER) {

                if (packet.flags == 0) {
                    sendBuffer[packet.seqNumber] = packet;
                    timers[packet.seqNumber] = GetTimeNow();

                }
            }

        }

    }

    return true;

}

NetworkPacket ReceivePacket(SOCKET socket, sockaddr_in& address)
{
	NetworkPacket packet;

    int addressSize = sizeof(address);
    int receivedBytes = recvfrom(socket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&address, &addressSize);

	if (receivedBytes == SOCKET_ERROR)
	{
        size_t errorCode = WSAGetLastError();

		std::cerr << "Failed to receive data. Error: " << errorCode << std::endl;

        packet.packetID = UINT16_MAX;

	} else {

        uint32_t seqNum = packet.seqNumber;

        if ((seqNum >= recvBase && seqNum < recvBase + WIND_SIZE) ||
            (seqNum < recvBase && (seqNum + SEQ_NUM_SPACE) < (recvBase + WIND_SIZE))) {

            std::cout << "\nReceived a packet!\n";
            std::cout << "Sequence number: " << seqNum << "\n";
            std::cout << "Packet ID: " << std::to_string(packet.packetID) << "\n";
            std::cout << "Flag: " << std::to_string(packet.flags) << "\n\n";

            recvBuffer[seqNum] = packet;

            if (packet.flags == 0) {
                // Slide window forward when contiguous packets are received
                while (recvBuffer.count(recvBase)) {
                    recvBuffer.erase(recvBase);
                    recvBase = (recvBase + 1) % SEQ_NUM_SPACE;
                }
            }
            else if (packet.flags == 1 && networkType == NetworkType::CLIENT) {

                if (sendBuffer.find(packet.seqNumber) != sendBuffer.end()) {

                    ackedPackets.insert(packet.seqNumber);

                    // Advance sendBase if the lowest unacknowledged packet is now acknowledged
                    while (ackedPackets.count(sendBase)) {
                        std::cout << "\nSliding window: removing " << sendBase << std::endl;
                        sendBuffer.erase(sendBase);
                        timers.erase(sendBase);
                        ackedPackets.erase(sendBase);
                        sendBase = (sendBase + 1) % SEQ_NUM_SPACE;
                    }
                }
            }

        }
    }

	return packet;
}

void RetransmitPacket() {

    std::map<uint32_t, uint64_t> accumulative_timers{};
    for (auto& [seqNum, time] : timers) {
        accumulative_timers[seqNum] = 0;
    }

    // Retransmission
    uint64_t now = GetTimeNow();
    for (auto& [seqNum, timer] : timers) {

        if (now - timer >= TIMEOUT_MS && accumulative_timers[seqNum] < TIMEOUT_MS_MAX) {

            //std::cout << "Timeout resending packet with sequence number: " << seqNum << std::endl;

            SendPacket(udpClientSocket, serverTargetAddress, sendBuffer[seqNum], true);

            accumulative_timers[seqNum] += (now - timer);
            timers[seqNum] = now;  // Update retransmission time

        } else if (accumulative_timers[seqNum] >= TIMEOUT_MS_MAX) {

            // Receiver is unresponsive, exit loop
            std::cout << "Receiver unresponsive. Resetting connection.\n";
            sendBuffer.clear();
            timers.clear();
            ackedPackets.clear();
            accumulative_timers.clear();
            nextSeqNum = sendBase;
            return;
        }
    }
}

bool SendAck(SOCKET socket, sockaddr_in address, NetworkPacket packet) {

    packet.flags = 1;

    int sentBytes = sendto(socket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&address, sizeof(address));

    if (sentBytes == SOCKET_ERROR) {

        std::cerr << "Failed to send ACK. Error: " << WSAGetLastError() << std::endl;

        return false;

    }
    return true;
}

void HandleConnectionRequest(SOCKET socket, sockaddr_in address, NetworkPacket packet) {

    if (packet.packetID == REQ_CONNECT && packet.flags == 0) {
        std::cout << "Server is acknowledging connnection request" << std::endl;
        SendAck(socket, address, packet);
    }
}

void SendQuitRequest(SOCKET socket, sockaddr_in address) {

    NetworkPacket packet{};
    packet.packetID = PacketID::REQ_QUIT;
    packet.flags = 0;
    packet.sourcePortNumber = clientPort;
    packet.destinationPortNumber = address.sin_port;
    SendPacket(socket, address, packet, false);
}

// Pack the player data into the network packet for sending
void PackPlayerData(NetworkPacket& packet, const PlayerData& player)
{
	std::lock_guard<std::mutex> lock(packetMutex);
	memset(packet.data, 0, DEFAULT_BUFLEN); // Ensure buffer is cleared
	memcpy(packet.data, &player, sizeof(PlayerData)); // Copy struct into buffer
}

void PackGameStateData(NetworkPacket& packet, const NetworkGameState& gameState)
{
	std::lock_guard<std::mutex> lock1(packetMutex);
	std::lock_guard<std::mutex> lock2(gameDataMutex);
	memset(packet.data, 0, DEFAULT_BUFLEN); // Ensure buffer is cleared
	memcpy(packet.data, &gameState, sizeof(NetworkGameState)); // Copy struct into buffer
}


// Unpacking the data from the network packet into the player data
void UnpackPlayerData(const NetworkPacket& packet, PlayerData& player)
{
	std::lock_guard<std::mutex> lock(packetMutex);
	
	memcpy(&player, packet.data, sizeof(PlayerData)); // Copy buffer back into struct
}


// Unpacking the data from the network packet into the player data
void UnpackGateStateData(const NetworkPacket& packet)
{
	std::lock_guard<std::mutex> lock1(packetMutex);
	std::lock_guard<std::mutex> lock2(gameDataMutex);
	memcpy(&gameDataState, packet.data, sizeof(NetworkGameState)); // Copy buffer back into struct
}


void SendJoinRequest(SOCKET socket, sockaddr_in address) 
{
    std::cout << "Client is sending join request..." << std::endl;
	NetworkPacket packet;
	packet.packetID = PacketID::JOIN_REQUEST;
	packet.sourcePortNumber = clientPort;
	packet.destinationPortNumber = address.sin_port;
	SendPacket(socket, address, packet);
}

void HandleJoinRequest(SOCKET socket, sockaddr_in address, NetworkPacket packet)
{
	if (packet.packetID == PacketID::JOIN_REQUEST) 
	{
		std::cout << "Player [" << packet.sourcePortNumber << "] is joining the lobby." << std::endl;

		NetworkPacket responsePacket;
		responsePacket.packetID = PacketID::REQUEST_ACCEPTED;
		responsePacket.sourcePortNumber = serverPort;
		responsePacket.destinationPortNumber = packet.sourcePortNumber;
        SendAck(socket, address, packet);
		SendPacket(socket, address, responsePacket);
       
	}
}

void SendInput(SOCKET socket, sockaddr_in address) 
{
	NetworkPacket packet;
	packet.packetID = PacketID::GAME_INPUT;
	packet.sourcePortNumber = serverPort;
	packet.destinationPortNumber = address.sin_port;
	strcpy_s(packet.data, "[PLAYER_INPUT_DATA]");
	SendPacket(socket, address, packet);
}

uint16_t GetClientPort()
{
	return clientPort;
}

void HandleClientInput(SOCKET serverUDPSocket, uint16_t clientPortID, std::map<uint16_t, PlayerData>& playersData)
{
	while (true)
	{
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(serverUDPSocket, &readSet);

		timeval timeout{};
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000; // 10ms timeout

		int activity = select(0, &readSet, nullptr, nullptr, &timeout);

		if (activity > 0) // Data available
		{
			sockaddr_in senderAddress{};
			NetworkPacket gamePacket = ReceivePacket(serverUDPSocket, senderAddress);

			if (gamePacket.packetID == UINT16_MAX) // Check for invalid packet
				continue;

            SendAck(serverUDPSocket, senderAddress, gamePacket);

			// Ensure this is from the correct client
			if (gamePacket.sourcePortNumber != clientPortID)
				continue;

			// Ensure the player's data exists
			if (playersData.count(clientPortID))
			{
				HandlePlayerInput(clientPortID, gamePacket);
			}
			else
			{
				std::cerr << "Warning: Received input from unregistered player " << clientPortID << std::endl;
				return;
			}
		}

		// Small sleep to prevent CPU from running at 100%
		Sleep(1);
	}
}

void HandlePlayerInput(uint16_t clientPortID, NetworkPacket& packet)
{
	// Note: Uncomment to test if the keys are working correctly
	std::lock_guard<std::mutex> lock(playerDataMutex);
	PlayerInput& playerInput = playerInputMap[clientPortID];
	if (packet.packetID == InputKey::NONE)
	{
		//playersData[clientPortID].transform.velocity = { 0, 0 };
		playerInput.NoInput();
	}
	else if (packet.packetID == InputKey::UP)
	{
		//playersData[clientPortID].transform.position = { 10, 10 };
		playerInput.upKey = true;
	}
	else if (packet.packetID == InputKey::DOWN)
	{
		//playersData[clientPortID].transform.position = { 20, 20 };
		playerInput.downKey = true;
	}
	else if (packet.packetID == InputKey::RIGHT)
	{
		//playersData[clientPortID].transform.position = { 30, 30 };
		playerInput.rightKey = true;
	}
	else if (packet.packetID == InputKey::LEFT)
	{
		//playersData[clientPortID].transform.position = { 40, 40 };
		playerInput.leftKey = true;
	}
	else if (packet.packetID == InputKey::SPACE)
	{
		playerInput.spaceKey = true;
    }
}

void SendGameStateStart(SOCKET socket, sockaddr_in address, PlayerData& playerData)
{
	NetworkPacket packet;
	packet.packetID = PacketID::GAME_STATE_START;
	packet.sourcePortNumber = serverPort;
	packet.destinationPortNumber = address.sin_port;
	PackPlayerData(packet, playerData);
	SendPacket(socket, address, packet);
}

void ReceiveGameStateStart(SOCKET socket, PlayerData& player, NetworkPacket packet)
{
    (void)socket;
    //sockaddr_in address{};
    //NetworkPacket packet = ReceivePacket(socket, address);

    if (packet.packetID == PacketID::GAME_STATE_START)
    {
        std::cout << "Game started. Initial game state: " << packet.data << std::endl;
        UnpackPlayerData(packet, player);
        std::cout << "Initial Player Position: " << player.transform.position.x << " " << player.transform.position.y << std::endl;
    }
}


void BroadcastGameState(SOCKET socket, std::map<uint16_t, sockaddr_in>& clients)
{
	while (true)
	{
		Sleep(16); // Approx 60 updates per second (1000ms / 60)

		// 1. **Send the full game state to all clients**
		NetworkPacket responsePacket;
		responsePacket.packetID = PacketID::GAME_STATE_UPDATE;
		responsePacket.sourcePortNumber = serverPort;					// Server's port

		PackGameStateData(responsePacket, gameDataState);

		for (auto& [portID, clientAddr] : clients)
		{
			{
				responsePacket.destinationPortNumber = portID;			// Client's port
				SendPacket(socket, clientAddr, responsePacket);
			}
		}

	}
}

// Thread function to receive packets continuously
void ListenForUpdates(SOCKET socket, sockaddr_in serverAddr, PlayerData& player)
{
	while (true)
	{
		NetworkPacket receivedPacket = ReceivePacket(socket, serverAddr);
		if (receivedPacket.packetID == GAME_STATE_UPDATE)
		{
			UnpackGateStateData(receivedPacket);
			for (int i = 0; i < static_cast<int>(gameDataState.objectCount); ++i)
			{
				if (gameDataState.objects[i].type == ObjectType::OBJ_SHIP && gameDataState.objects[i].identifier == clientPort)
				{
					player.transform = gameDataState.objects[i].transform;
					for (int j = 0; j < static_cast<int>(gameDataState.playerCount); ++j)
					{
						if (gameDataState.playerData[j].identifier == clientPort)
						{
							player.stats = gameDataState.playerData[j];
						}
					}
				}
			}

        } else if (receivedPacket.packetID == REQUEST_ACCEPTED) {

            std::cout << "Joined the lobby successfully!" << std::endl;
            std::cout << "Waiting for lobby to start..." << std::endl;

            gGameStateNext = GS_LOBBY;
            
        } else if (receivedPacket.packetID == GAME_STATE_START) {

            ReceiveGameStateStart(udpClientSocket, clientData, receivedPacket);
            gGameStateNext = GS_ASTEROIDS;

        }
		std::cout << "Pos: " << player.transform.position.x << " " << player.transform.position.y << std::endl;
	}

}


void GameLoop(std::map<uint16_t, sockaddr_in>& clients)
{

	UNREFERENCED_PARAMETER(clients);

	while (true)
	{


		// Set the player data inside the gameState
		for (auto& [portID, playerData] : playerDataMap)
		{
			for (int i = 0; i < static_cast<int>(gameDataState.objectCount); ++i)
			{
				if (gameDataState.objects[i].type == ObjectType::OBJ_SHIP && gameDataState.objects[i].identifier == portID)
				{
					gameDataState.objects[i].transform = playerData.transform;
					for (int j = 0; j < static_cast<int>(gameDataState.playerCount); ++j)
					{
						if (gameDataState.playerData[j].identifier == portID)
						{
							gameDataState.playerData[j] = playerData.stats;
							break;
						}
					}
					break;
				}
			}
			std::cout << "Client: " << portID << " position: " << playerData.transform.position.x << " " << playerData.transform.position.y << std::endl;
		}
		

	}

}

void PackLeaderboardData(NetworkPacket& packet)
{
	std::lock_guard<std::mutex> lock1(packetMutex);
	std::lock_guard<std::mutex> lock2(leaderboardMutex);
	memset(packet.data, 0, DEFAULT_BUFLEN);							// Ensure buffer is cleared
	memcpy(packet.data, &leaderboard, sizeof(NetworkLeaderboard));	// Copy struct into buffer
}

// Unpacking the data from the network packet into the leaderboard
void UnpackLeaderboardData(const NetworkPacket& packet)
{
	std::lock_guard<std::mutex> lock1(packetMutex);
	std::lock_guard<std::mutex> lock2(gameDataMutex);
	memcpy(&gameDataState, packet.data, sizeof(NetworkLeaderboard)); // Copy buffer back into struct
}

void BroadcastLeaderboard(SOCKET socket, std::map<uint16_t, sockaddr_in>& clients)
{
	NetworkPacket responsePacket;
	responsePacket.packetID = PacketID::LEADERBOARD;
	responsePacket.sourcePortNumber = serverPort;					// Server's port

	PackLeaderboardData(responsePacket);

	for (auto& [portID, clientAddr] : clients)
	{
		{
			responsePacket.destinationPortNumber = portID;			// Client's port
			SendPacket(socket, clientAddr, responsePacket);
		}
	}
}

void ReceiveLeaderboard(SOCKET socket)
{
	sockaddr_in address{};
	NetworkPacket packet = ReceivePacket(socket, address);

	if (packet.packetID == PacketID::LEADERBOARD)
	{
		UnpackLeaderboardData(packet);
	}

	// Leaderboard is updated, can be shown to the players
}

void Render(NetworkGameState& gameState)
{
	UNREFERENCED_PARAMETER(gameState);

    // Initialize the system
    AESysInit(g_instanceH, g_show, 800, 600, 1, 60, false, NULL);

    pFont = (int)AEGfxCreateFont("Resources/Arial Italic.ttf", Fontsize);

    // Changing the window title
    AESysSetWindowTitle("Asteroids!");

    //set background color
    AEGfxSetBackgroundColor(0.0f, 0.0f, 0.0f);

    // set starting game state to asteroid
    //GameStateMgrInit(GS_ASTEROIDS);
    GameStateMgrInit(GS_MAINMENU);  // Start at the main Menu

    // breaks this loop if game state set to quit
    while (gGameStateCurr != GS_QUIT)
    {
        // reset the system modules
        AESysReset();

        // If not restarting, load the gamestate
        if (gGameStateCurr != GS_RESTART)
        {
            GameStateMgrUpdate();
            GameStateLoad();
        }
        else
        {
            gGameStateNext = gGameStateCurr = gGameStatePrev;
        }

        // Initialize the gamestate
        GameStateInit();

        // main game loop
        while (gGameStateCurr == gGameStateNext) {

            AESysFrameStart(); // start of frame

            GameStateUpdate(); // update current game state

            GameStateDraw(); // draw current game state

            AESysFrameEnd(); // end of frame

            // check if forcing the application to quit
            if (AESysDoesWindowExist() == false)
            {
                gGameStateNext = GS_QUIT;
            }

            g_dt = static_cast<f32>(AEFrameRateControllerGetFrameTime()); // get delta time
            g_appTime += g_dt; // accumulate application time
        }

        GameStateFree(); // free current game state

        // unload current game state unless set to restart
        if (gGameStateNext != GS_RESTART)
            GameStateUnload();

        // set prev and curr for the next game states
        gGameStatePrev = gGameStateCurr;
        gGameStateCurr = gGameStateNext;
    }

    // free the system
    AESysExit();
}

uint64_t GetTimeNow() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
}