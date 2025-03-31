#include "Network.h"
#include "taskqueue.h"




// Define
NetworkType networkType = NetworkType::UNINITIALISED;
sockaddr_in targetAddress;
uint16_t port;
uint16_t clientPort;
SOCKET udpSocket;
std::mutex packetMutex;



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
	std::string portString;
	std::cout << "Server UDP Port Number: ";
	std::getline(std::cin, portString);
	port = static_cast<uint16_t>(std::stoi(portString));

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
	if (getaddrinfo(nullptr, portString.c_str(), &udpHints, &udpInfo) != 0)
	{
		std::cerr << "getaddrinfo() failed. Error: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return ERROR_CODE;
	}

	// Create a UDP socket
	udpSocket = socket(udpInfo->ai_family, udpInfo->ai_socktype, udpInfo->ai_protocol);
	if (udpSocket == INVALID_SOCKET)
	{
		std::cerr << "socket() failed." << std::endl;
		freeaddrinfo(udpInfo);
		WSACleanup();
		return ERROR_CODE;
	}

	// Create a sockaddr_in for binding
	sockaddr_in bindAddress{};
	bindAddress.sin_family = AF_INET;
	bindAddress.sin_port = htons(port);
	bindAddress.sin_addr.s_addr = INADDR_ANY;

	if (bind(udpSocket, (sockaddr*)&bindAddress, sizeof(bindAddress)) == SOCKET_ERROR)
	{
		std::cerr << "bind() failed." << std::endl;
		closesocket(udpSocket);
		freeaddrinfo(udpInfo);
		WSACleanup();
		return ERROR_CODE;
	}

	// Print server IP address and port number
	char serverIPAddress[DEFAULT_BUFLEN];
	struct sockaddr_in* address = reinterpret_cast<struct sockaddr_in*> (udpInfo->ai_addr);
	inet_ntop(AF_INET, &(address->sin_addr), serverIPAddress, INET_ADDRSTRLEN);
	getnameinfo(udpInfo->ai_addr, static_cast <socklen_t>(udpInfo->ai_addrlen), serverIPAddress, sizeof(serverIPAddress), nullptr, 0, NI_NUMERICHOST);
	
	std::cout << std::endl;
	std::cout << "Server has been established" << std::endl;
	std::cout << "Server IP Address: " << serverIPAddress << std::endl;
	std::cout << "Server UDP Port Number: " << port << std::endl;
	std::cout << std::endl;

	freeaddrinfo(udpInfo);

	return 0;
}

int ConnectToServer()
{
	// Get IP Address
	std::string serverIPAddress{};
	std::cout << "Server IP Address: ";
	std::getline(std::cin, serverIPAddress);

	// Get UDP Server Port Number
	std::string serverPortString;
	std::cout << "Server UDP Port Number: ";
	std::getline(std::cin, serverPortString);
	uint16_t serverPort = static_cast<uint16_t>(std::stoi(serverPortString));

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
	udpSocket = socket(udpInfo->ai_family, udpInfo->ai_socktype, udpInfo->ai_protocol);
	if (udpSocket == INVALID_SOCKET)
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

	if (bind(udpSocket, (sockaddr*)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR)
	{
		std::cerr << "bind() failed. Error: " << WSAGetLastError() << std::endl;
		closesocket(udpSocket);
		freeaddrinfo(udpInfo);
		WSACleanup();
		return ERROR_CODE;
	}

	// Store target address
	targetAddress = *(reinterpret_cast<sockaddr_in*>(udpInfo->ai_addr));

	// Print server IP address and port number
	std::cout << std::endl;
	std::cout << "Client has been established" << std::endl;
	std::cout << "Server IP Address: " << serverIPAddress << std::endl;
	std::cout << "Server UDP Port Number: " << serverPort << std::endl;
	std::cout << "Client UDP Port Number: " << clientPort << std::endl;
	std::cout << std::endl;

	freeaddrinfo(udpInfo);
	return 0;
}

void Disconnect()
{
	shutdown(udpSocket, SD_SEND);
	closesocket(udpSocket);
	WSACleanup();
}

void SendPacket(SOCKET socket, sockaddr_in address, NetworkPacket packet)
{
	int sentBytes = sendto(socket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&address, sizeof(address));

	if (sentBytes == SOCKET_ERROR)
	{
		std::cerr << "Failed to send game data. Error: " << WSAGetLastError() << std::endl;
	}
}

NetworkPacket ReceivePacket(SOCKET socket, sockaddr_in& address)
{
	NetworkPacket packet;

    int addressSize = sizeof(address);
    int receivedBytes = recvfrom(socket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&address, &addressSize);

	if (receivedBytes == SOCKET_ERROR)
	{
		std::cerr << "Failed to receive data. Error: " << WSAGetLastError() << std::endl;
		packet.packetID = UINT16_MAX;
	}

	return packet;
}

// Pack the player data into the network packet for sending
void PackPlayerData(NetworkPacket& packet, const PlayerData& player)
{
	static_assert(sizeof(PlayerData) <= DEFAULT_BUFLEN, "PlayerData is too large for NetworkPacket data buffer!");

	std::lock_guard<std::mutex> lock(packetMutex); // Lock the mutex to prevent race conditions

	memset(packet.data, 0, DEFAULT_BUFLEN); // Ensure buffer is cleared
	memcpy(packet.data, &player, sizeof(PlayerData)); // Copy struct into buffer
	std::cout << "Unpacked: PosX=" << player.posX << ", PosY=" << player.posY
		<< ", VelX=" << player.velX << ", VelY=" << player.velY << std::endl;
}


// Unpacking the data from the network packet into the player data
void UnpackPlayerData(const NetworkPacket& packet, PlayerData& player)
{
	memcpy(&player, packet.data, sizeof(PlayerData)); // Copy buffer back into struct
}

void SendJoinRequest(SOCKET socket, sockaddr_in address) 
{
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
		responsePacket.sourcePortNumber = port;
		responsePacket.destinationPortNumber = packet.sourcePortNumber;
		SendPacket(socket, address, responsePacket);
	}
}

void SendInput(SOCKET socket, sockaddr_in address) 
{
	NetworkPacket packet;
	packet.packetID = PacketID::GAME_INPUT;
	packet.sourcePortNumber = port;
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
		sockaddr_in senderAddress{};
		NetworkPacket gamePacket = ReceivePacket(serverUDPSocket, senderAddress);

		if (gamePacket.packetID == UINT16_MAX) // Check for invalid packet
			continue;

		// Ensure this is from the correct client
		if (gamePacket.sourcePortNumber != clientPortID)
			continue;

		// Ensure the player's data exists
		if (playersData.count(clientPortID))
		{
			HandlePlayerInput(clientPortID, gamePacket, playersData);
		}
		else
		{
			std::cerr << "Warning: Received input from unregistered player " << clientPortID << std::endl;
			return;
		}
	}
}

void HandlePlayerInput(uint16_t clientPortID, NetworkPacket packet, std::map<uint16_t, PlayerData>& playersData)
{
	float speed = 0;
	if (packet.packetID == InputKey::NONE)
	{
		playersData[clientPortID].velX = 0;
		playersData[clientPortID].velY = 0;
	}
	else if (packet.packetID == InputKey::UP)
	{
		float angle = degToRad(playersData[clientPortID].rotation);
		playersData[clientPortID].velX = cosf(angle);
		playersData[clientPortID].velY =  sinf(angle);
		speed = 10;
		//std::cout << "UP" << std::endl;
	}
	else if (packet.packetID == InputKey::DOWN)
	{
		float angle = degToRad(playersData[clientPortID].rotation);
		playersData[clientPortID].velX = -cosf(angle);
		playersData[clientPortID].velY = -sinf(angle);
		speed = 10;
		//std::cout << "down" << std::endl;
	}
	else if (packet.packetID == InputKey::RIGHT)
	{
		playersData[clientPortID].rotation += 0.5;
		//std::cout << "right" << std::endl;
	}
	else if (packet.packetID == InputKey::LEFT)
	{
		playersData[clientPortID].rotation -= 0.5;
		//std::cout << "left" << std::endl;
	}
	else if (packet.packetID == InputKey::SPACE)
	{

	}
	else
	{
		playersData[clientPortID].velX = 0;
		playersData[clientPortID].velY = 0;
	}

	// **Apply velocity to update position**
	playersData[clientPortID].posX += playersData[clientPortID].velX * 0.016f * speed;
	playersData[clientPortID].posY += playersData[clientPortID].velY * 0.016f * speed;
}

void SendGameStateStart(SOCKET socket, sockaddr_in address, PlayerData& playerData)
{
	NetworkPacket packet;
	packet.packetID = PacketID::GAME_STATE_START;
	packet.sourcePortNumber = port;
	packet.destinationPortNumber = address.sin_port;
	PackPlayerData(packet, playerData);
	SendPacket(socket, address, packet);
}

void ReceiveGameStateStart(SOCKET socket, PlayerData& clientData) 
{
	sockaddr_in address{};
	NetworkPacket packet = ReceivePacket(socket, address);

	if (packet.packetID == PacketID::GAME_STATE_START)
	{
		std::cout << "Game started. Initial game state: " << packet.data << std::endl;
		UnpackPlayerData(packet, clientData);
		std::cout << "Initial Player Position: " << clientData.posX << " " << clientData.posY << std::endl;
	}
}


void BroadcastGameState(SOCKET socket, std::map<uint16_t, sockaddr_in>& clients, std::map<uint16_t, PlayerData>& playersData)
{
	while (true)
	{
		Sleep(16); // Approx 60 updates per second (1000ms / 60)

		for (auto& [portID, clientAddr] : clients)
		{
			if (playersData.count(portID) == 0)
				continue; // Skip if no player data

			NetworkPacket responsePacket;
			responsePacket.packetID = PacketID::GAME_STATE_UPDATE;
			responsePacket.sourcePortNumber = port;			// Server's port
			responsePacket.destinationPortNumber = portID;	// Client's port

			PackPlayerData(responsePacket, playersData[portID]);
			SendPacket(socket, clientAddr, responsePacket);
		}
	}
}

// Thread function to receive packets continuously
void ListenForUpdates(SOCKET socket, sockaddr_in serverAddr, PlayerData& clientData)
{
	while (true)
	{
		NetworkPacket receivedPacket = ReceivePacket(socket, serverAddr);
		if (receivedPacket.packetID == GAME_STATE_UPDATE)
		{
			UnpackPlayerData(receivedPacket, clientData);
			std::cout << "New Position: " << clientData.posX << " " << clientData.posY << std::endl;
		}
	}
}