#include "Network.h"
#include "Collision.h"


// Define
NetworkType networkType = NetworkType::UNINITIALISED;
//sockaddr_in targetAddress;
sockaddr_in serverTargetAddress{};                              // used for NetworkType::CLIENT
sockaddr_in clientTargetAddress{};								// used for NetworkType::SERVER
uint16_t serverPort;
uint16_t clientPort;
//SOCKET udpSocket;
SOCKET udpClientSocket = INVALID_SOCKET;                        // used for NetworkType::CLIENT
SOCKET udpServerSocket = INVALID_SOCKET;                        // used for NetworkType::SERVER
std::mutex packetMutex;
std::mutex playerDataMutex;

const std::string configFileRelativePath = "Resources/Configuration.txt";
const std::string configFileServerIp = "serverIp";
const std::string configFileServerPort = "serverUdpPort";
bool clientRunning = true;


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
	return 0;
}

void Disconnect(SOCKET& socket)
{
	shutdown(socket, SD_SEND);
	closesocket(socket);
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

		// Small sleep to prevent CPU from running at 100%
		//Sleep(1);
	}
}

void HandlePlayerInput(uint16_t clientPortID, NetworkPacket& packet, std::map<uint16_t, PlayerData>& playersData)
{
	UNREFERENCED_PARAMETER(playersData);
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

void ReceiveGameStateStart(SOCKET socket, PlayerData& player)
{
	sockaddr_in address{};
	NetworkPacket packet = ReceivePacket(socket, address);

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
		//Sleep(16); // Approx 60 updates per second (1000ms / 60)

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
	while (clientRunning)
	{
		NetworkPacket receivedPacket = ReceivePacket(socket, serverAddr);
		if (receivedPacket.packetID == GAME_STATE_UPDATE)
		{
			UnpackGateStateData(receivedPacket);
			for (int i = 0; i < MAX_NETWORK_OBJECTS; ++i)
			{
				if (gameDataState.objects[i].type == 1 && gameDataState.objects[i].identifier == clientPort)
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
		}
		//std::cout << "Pos: " << player.transform.position.x << " " << player.transform.position.y << std::endl;
	}

}

void PackLeaderboardData(NetworkPacket& packet)
{
	std::lock_guard<std::mutex> lock1(packetMutex);
	std::lock_guard<std::mutex> lock2(leaderboardMutex);
	memset(packet.data, 0, DEFAULT_BUFLEN);                            // Ensure buffer is cleared
	memcpy(packet.data, &leaderboard, sizeof(NetworkLeaderboard));    // Copy struct into buffer
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
	responsePacket.sourcePortNumber = serverPort;                    // Server's port

	PackLeaderboardData(responsePacket);

	for (auto& [portID, clientAddr] : clients)
	{
		{
			responsePacket.destinationPortNumber = portID;            // Client's port
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

void GameLoop(std::map<uint16_t, sockaddr_in>& clients)
{
	UNREFERENCED_PARAMETER(clients);
	using namespace std::chrono;
	auto prevTime = high_resolution_clock::now();

	while (clientRunning)
	{
		auto currTime = high_resolution_clock::now();
		float deltaTime = duration<float>(currTime - prevTime).count();
		prevTime = currTime;

		std::lock_guard<std::mutex> lock(gameDataMutex);

		for (auto& [portID, playerData] : playerDataMap)
		{
			PlayerInput& input = playerInputMap[portID];

			// rotate
			float rotationSpeed = 3.0f; // radians/sec
			if (input.leftKey)
				playerData.transform.rotation += rotationSpeed * deltaTime;
			if (input.rightKey)
				playerData.transform.rotation -= rotationSpeed * deltaTime;

			// front back
			float thrustSpeed = 200.0f;
			AEVec2 forward{ cosf(playerData.transform.rotation), sinf(playerData.transform.rotation) };

			if (input.upKey)
				playerData.transform.velocity += forward * thrustSpeed * deltaTime;
			if (input.downKey)
				playerData.transform.velocity -= forward * thrustSpeed * deltaTime;

			// Update position using velocity
			playerData.transform.position += playerData.transform.velocity * deltaTime;

			// Wrap the ship from one end of the screen to the other
			playerData.transform.position.x = AEWrap(playerData.transform.position.x, -400 - playerData.transform.scale.x,
				400 + playerData.transform.scale.x);
			playerData.transform.position.y = AEWrap(playerData.transform.position.y, -300 - playerData.transform.scale.y,
				300 + playerData.transform.scale.y);

			// space key
			if (input.spaceKey == true)
			{
				NetworkTransform bullet{};
				bullet.position = playerData.transform.position;
				bullet.rotation = playerData.transform.rotation;
				bullet.velocity = forward * 400.0f; // bullet speed
				bullet.scale = { 5.0f, 5.0f };
				
				for (int x = 0; x < MAX_NETWORK_OBJECTS; ++x)
				{
					if (gameDataState.objects[x].type == (int)ObjectType::OBJ_NULL)
					{
						//playerBulletMap[portID].push_back(bullet);
						gameDataState.objects[x].identifier = clientPort;
						gameDataState.objects[x].transform = bullet;
						gameDataState.objects[x].type = (int)ObjectType::OBJ_BULLET;
						break;
					}
				}
				//gameDataState.objects[gameDataState.objectCount].identifier = serverPort;
				//gameDataState.objects[gameDataState.objectCount].transform = bullet;
				//gameDataState.objects[gameDataState.objectCount].type = (int)ObjectType::OBJ_BULLET;
				//++gameDataState.objectCount;

				input.spaceKey = false; // Prevent continuous firing
			}

			// Update game state (ship transform & stats)
			for (int i = 0; i < MAX_NETWORK_OBJECTS; ++i)
			{
				if (gameDataState.objects[i].type == (int)ObjectType::OBJ_SHIP && gameDataState.objects[i].identifier == portID)
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

			//std::cout << "Client: " << portID << " Pos: " << playerData.transform.position.x << " " << playerData.transform.position.y << std::endl;
		}

		for (int i = 0; i < MAX_NETWORK_OBJECTS; ++i)
		{
			if (gameDataState.objects[i].type == (int)ObjectType::OBJ_ASTEROID)
			{
				gameDataState.objects[i].transform.position += gameDataState.objects[i].transform.velocity * deltaTime;
				gameDataState.objects[i].transform.position.x = 
					AEWrap(gameDataState.objects[i].transform.position.x, -400 - gameDataState.objects[i].transform.scale.x,
					400 + gameDataState.objects[i].transform.scale.x);
				gameDataState.objects[i].transform.position.y = 
					AEWrap(gameDataState.objects[i].transform.position.y, -300 - gameDataState.objects[i].transform.scale.y,
					300 + gameDataState.objects[i].transform.scale.y);
			}

			if (gameDataState.objects[i].type == (int)ObjectType::OBJ_BULLET)
			{
				gameDataState.objects[i].transform.position += gameDataState.objects[i].transform.velocity * deltaTime;

				float minX = -400 - gameDataState.objects[i].transform.scale.x;
				float maxX = 400 + gameDataState.objects[i].transform.scale.x;
				float minY = -300 - gameDataState.objects[i].transform.scale.y;
				float maxY = 300 + gameDataState.objects[i].transform.scale.y;

				if (gameDataState.objects[i].transform.position.x < minX || gameDataState.objects[i].transform.position.x > maxX ||
					gameDataState.objects[i].transform.position.y < minY || gameDataState.objects[i].transform.position.y > maxY)
				{
					gameDataState.objects[i].type = (int)ObjectType::OBJ_NULL;
				}
			}
		}
		// limit server loop rate
		//std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
}
