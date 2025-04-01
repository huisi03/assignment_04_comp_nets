#include "Network.h"

// Define
NetworkType networkType = NetworkType::UNINITIALISED;
uint32_t nextSeqNum = SEQ_NUM_MIN;
uint32_t sendBase = SEQ_NUM_MIN;
uint32_t recvBase = SEQ_NUM_MIN;
std::map<uint32_t, NetworkPacket> sendBuffer{};                 // Packets awaiting ACK
std::map<uint32_t, NetworkPacket> recvBuffer{};                 // Stores received packets
std::set<uint32_t> ackedPackets;                                // Tracks received ACKs
std::map<uint32_t, uint64_t> timers{};                          // Timeout tracking of un-ACK packets
std::vector<sockaddr_in> clientTargetAddresses;                 // used for NetworkType::SERVER
std::vector<sockaddr_in> clientsJoiningGame;                    // used for NetworkType::SERVER
sockaddr_in serverTargetAddress{};                              // used for NetworkType::CLIENT
uint16_t serverPort{};                                          // used for NetworkType::CLIENT
uint16_t clientPort{};                                          // used for NetworkType::SERVER
SOCKET udpClientSocket = INVALID_SOCKET;                        // used for NetworkType::CLIENT
SOCKET udpServerSocket = INVALID_SOCKET;                        // used for NetworkType::SERVER

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

	if (networkTypeString == "s")
	{
		networkType = NetworkType::SERVER;
		std::cout << "Initialising as Server..." << std::endl;
		return StartServer();
	}
	else if (networkTypeString == "c") 
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

    u_long mode = 1;  // 1 = Non-blocking
    ioctlsocket(udpServerSocket, FIONBIO, &mode);

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

	// Print server IP address and port number
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
		WSACleanup();
		return ERROR_CODE;
	}

    u_long mode = 1;  // 1 = Non-blocking
    ioctlsocket(udpClientSocket, FIONBIO, &mode);

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
    std::cout << "\nClient is sending a request connection to server..." << std::endl;
    NetworkPacket requestConnectionSegment{};
    requestConnectionSegment.commandID = CommandID::REQ_CONNECT;
    requestConnectionSegment.flags = 0;
    requestConnectionSegment.dataLength = 0;

    if (SendPacket(udpClientSocket, serverTargetAddress, requestConnectionSegment, false)) {
        // Await for ack of sent segment
        sockaddr_in address{};
        NetworkPacket packet = ReceivePacket(udpClientSocket, address);
    }
    else {
        return ERROR_CODE;
    }


    

	return 0;
}

void Disconnect()
{
    if (networkType == NetworkType::CLIENT) {

        shutdown(udpClientSocket, SD_SEND);
        closesocket(udpClientSocket);

    } else if (networkType == NetworkType::SERVER) {

        shutdown(udpServerSocket, SD_SEND);
        closesocket(udpServerSocket);
    }
	
	WSACleanup();
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

            std::cout << "Timeout resending packet with sequence number: " << seqNum << std::endl;

            SendPacket(udpClientSocket, serverTargetAddress, sendBuffer[seqNum], true);

            accumulative_timers[seqNum] += (now - timer);
            timers[seqNum] = now;  // Update retransmission time

        }
        else if (accumulative_timers[seqNum] >= TIMEOUT_MS_MAX) {

            // Receiver is unresponsive, exit loop
            std::cout << "Receiver unresponsive. Resetting connection.\n";
            sendBuffer.erase(seqNum);
            timers.erase(seqNum);
            ackedPackets.erase(seqNum);
            accumulative_timers.erase(seqNum);
            nextSeqNum = sendBase;
            return;
        }
    }
}

bool SendAck(SOCKET socket, sockaddr_in address, NetworkPacket packet) {
    packet.flags = 1;
    return SendPacket(socket, address, packet, false);
}

bool SendPacket(SOCKET socket, sockaddr_in address, NetworkPacket packet, bool is_retransmit) {

    if (is_retransmit) {

        int sentBytes = sendto(socket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&address, sizeof(address));

        if (sentBytes == SOCKET_ERROR) {

            std::cerr << "Failed to send game data. Error: " << WSAGetLastError() << std::endl;

            return false;
        }

        return true;
    }


    if ((nextSeqNum % SEQ_NUM_SPACE != (sendBase + WIND_SIZE) % SEQ_NUM_SPACE) || networkType == NetworkType::SERVER) {

        std::cout << std::endl;
        std::cout << "Sending packet of sequence number: " << nextSeqNum << std::endl;
        std::cout << "Command id is " << std::to_string(packet.commandID) << std::endl;
        std::cout << "Flag is " << std::to_string(packet.flags) << std::endl;
        std::cout << std::endl;

        packet.seqNumber = nextSeqNum;

        int sentBytes = sendto(socket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&address, sizeof(address));

        if (sentBytes == SOCKET_ERROR) {

            std::cerr << "Failed to send game data. Error: " << WSAGetLastError() << std::endl;

            return false;

        } else {

            nextSeqNum = (nextSeqNum + 1) % SEQ_NUM_SPACE;

            if (networkType == NetworkType::SERVER) {
                return true;
            }

            if (packet.flags == 0) {
                uint64_t now = GetTimeNow();
                sendBuffer[packet.seqNumber] = packet;
                timers[packet.seqNumber] = now;

            }

            return true;

        }
    
    }
}

NetworkPacket ReceivePacket(SOCKET socket, sockaddr_in& address) {

    NetworkPacket packet{};
    int addressSize = sizeof(address);
    int receivedBytes = recvfrom(socket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&address, &addressSize);

    if (receivedBytes == SOCKET_ERROR) {

        size_t errorCode = WSAGetLastError();

        std::cerr << "Failed to receive data. Error: " << errorCode << std::endl;

        if (errorCode != WSAEWOULDBLOCK)
        {
            packet.commandID = ERROR_PKT;
        }

    } else {

        uint32_t seqNum = packet.seqNumber;

        if ((seqNum >= recvBase && seqNum < recvBase + WIND_SIZE) ||
            (seqNum < recvBase && (seqNum + SEQ_NUM_SPACE) < (recvBase + WIND_SIZE))) {

            std::cout << "\nReceived a packet!\n";
            std::cout << "Sequence number: " << seqNum << "\n";
            std::cout << "Command ID: " << std::to_string(packet.commandID) << "\n";
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

void HandleConnectionRequest(SOCKET socket, sockaddr_in address, NetworkPacket packet) {

    if (packet.commandID == REQ_CONNECT && packet.flags == 0) {

        if (std::find_if(clientTargetAddresses.begin(), clientTargetAddresses.end(), [&](sockaddr_in target) {
            return CompareSockaddr(address, target);
            }) == clientTargetAddresses.end()) {

            if (SendAck(socket, address, packet)) {
                // Add client
                clientTargetAddresses.push_back(address);
                std::cout << "Number of clients connected: " << clientTargetAddresses.size() << std::endl;
            }

        }

    }
   
}

void SendQuitRequest(SOCKET socket, sockaddr_in address) {
    NetworkPacket packet{};
    packet.commandID = CommandID::REQ_QUIT;
    SendPacket(socket, address, packet, false);
}

void HandleQuitRequest(SOCKET socket, sockaddr_in address, NetworkPacket packet) {

    if (packet.commandID == REQ_QUIT && packet.flags == 0) {
        packet.flags = 1;
        SendPacket(socket, address, packet, false);
    }

    // Remove client
    auto it = std::remove_if(clientTargetAddresses.begin(), clientTargetAddresses.end(),
        [&](sockaddr_in target) {
            return CompareSockaddr(address, target);
        }
    );
    clientTargetAddresses.erase(it, clientTargetAddresses.end());

    it = std::remove_if(clientsJoiningGame.begin(), clientsJoiningGame.end(),
        [&](sockaddr_in target) {
            return CompareSockaddr(address, target);
        }
    );
    clientsJoiningGame.erase(it, clientsJoiningGame.end());
    std::cout << "Number of clients connected: " << clientTargetAddresses.size() << std::endl;
}


void SendJoinRequest(SOCKET socket, sockaddr_in address) {
	NetworkPacket packet;
	packet.commandID = REQ_GAME_START;
	SendPacket(socket, address, packet, false);

}

void HandleJoinRequest(SOCKET socket, sockaddr_in address, NetworkPacket packet)
{
	if (packet.commandID == REQ_GAME_START && packet.flags == 0) {

        if (std::find_if(clientTargetAddresses.begin(), clientTargetAddresses.end(), [&](sockaddr_in target) {
            return CompareSockaddr(address, target);
            }) != clientTargetAddresses.end()) {

            // Send ACK
            if (SendAck(socket, address, packet)) {

                uint16_t sourcePortNumber = address.sin_port;
                std::cout << "[SERVER SIDE] Player [" << sourcePortNumber << "] is joining the lobby." << std::endl;

                if (std::find_if(clientsJoiningGame.begin(), clientsJoiningGame.end(), [&](sockaddr_in target) {
                    return CompareSockaddr(address, target);
                    }) == clientsJoiningGame.end()) {
                    clientsJoiningGame.push_back(address);
                }
            }

            
        }
	}
}

void SendInput(SOCKET socket, sockaddr_in address) 
{
    (void)socket, address;
	/*NetworkPacket packet;
	packet.packetID = PacketID::GAME_INPUT;
	packet.sourcePortNumber = port;
	packet.destinationPortNumber = address.sin_port;
	strcpy_s(packet.data, "[PLAYER_INPUT_DATA]");
	SendPacket(socket, address, packet);*/
}

void HandlePlayerInput(SOCKET socket, sockaddr_in address, NetworkPacket packet)
{
    (void)socket, address, packet;
	/*if (packet.packetID == PacketID::GAME_INPUT)
	{
		std::cout << "Received input from Player [" << packet.sourcePortNumber << "]: " << packet.data << std::endl;

		NetworkPacket responsePacket;
		responsePacket.packetID = PacketID::GAME_STATE_UPDATE;
		responsePacket.sourcePortNumber = port;
		responsePacket.destinationPortNumber = packet.sourcePortNumber;
		strcpy_s(responsePacket.data, "[GAME_STATE_DATA]");

		SendPacket(socket, address, responsePacket);
	}*/
}

void SendGameStateStart(SOCKET socket, sockaddr_in address)
{
    // Send RSP_GAME_START
    NetworkPacket packet;
    packet.commandID = RSP_GAME_START;
    if (SendPacket(socket, address, packet, false)) {
        std::cout << "[SERVER SIDE] Game started. Initial game state..." << std::endl;
    }
    
    // @todo if needed: To send initial game data with the packet
}

bool HandleGameStateStart(NetworkPacket recvPkt)
{
    if (recvPkt.commandID == RSP_GAME_START && recvPkt.flags == 0) {
        // Send ACK
        if (SendAck(udpClientSocket, serverTargetAddress, recvPkt)) {
            std::cout << "[CLIENT SIDE] Game started. Initial game state..." << std::endl;
            return true;
        }
    }

    return false;

}

bool CompareSockaddr(const sockaddr_in& addr1, const sockaddr_in& addr2) {
    return (addr1.sin_family == addr2.sin_family &&
        addr1.sin_port == addr2.sin_port &&
        addr1.sin_addr.s_addr == addr2.sin_addr.s_addr);
}

uint64_t GetTimeNow() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
}