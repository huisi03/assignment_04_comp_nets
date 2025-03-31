#include "Network.h"

// Define
NetworkType networkType = NetworkType::UNINITIALISED;
uint32_t nextSeqNum = SEQ_NUM_MIN;
uint32_t sendBase = SEQ_NUM_MIN;
uint32_t recvBase = SEQ_NUM_MIN;
std::map<uint32_t, NetworkPacket> sendBuffer{};                 // Packets awaiting ACK
std::map<uint32_t, NetworkPacket> recvBuffer{};                 // Stores received packets
std::set<uint32_t> ackedPackets;                                // Tracks received ACKs
std::map<uint32_t, uint64_t> timers{};                          // Timeout tracking
std::map<uint32_t, sockaddr_in> clientTargetAddresses{};        // used for NetworkType::SERVER
sockaddr_in serverTargetAddress{};                              // used for NetworkType::CLIENT
uint16_t serverPort{};                                          // used for NetworkType::CLIENT
uint16_t clientPort{};                                          // used for NetworkType::SERVER
SOCKET udpClientSocket = INVALID_SOCKET;                        // used for NetworkType::CLIENT
SOCKET udpServerSocket = INVALID_SOCKET;                        // used for NetworkType::SERVER

const std::string configFileRelativePath = "Resources/Configuration";
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
        while (std::getline(std::cin, buffer)) {
            std::string::size_type findIndex = buffer.find(configFileServerPort);
            if (findIndex != std::string::npos) {
                portString = buffer.substr(findIndex + configFileServerPort.size());
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

	addrinfo* udpInfo = nullptr;
	if (getaddrinfo(nullptr, portString.c_str(), &udpHints, &udpInfo) != 0)
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
	bindAddress.sin_port = htons(udpServerSocket);
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
        while (std::getline(std::cin, buffer)) {
            std::string::size_type findIndex = buffer.find(configFileServerIp);
            if (findIndex != std::string::npos) {
                serverIPAddress = buffer.substr(findIndex + configFileServerIp.size());
            }
            findIndex = buffer.find(configFileServerPort);
            if (findIndex != std::string::npos) {
                serverPortString = buffer.substr(findIndex + configFileServerPort.size());
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
    std::cout << "Client is sending a request connection to server..." << std::endl;
    NetworkPacket requestConnectionSegment{};
    requestConnectionSegment.commandID = CommandID::REQ_CONNECT;
    requestConnectionSegment.flags = 0;
    requestConnectionSegment.seqNumber = htonl(sendBase); 
    requestConnectionSegment.dataLength = 0;

    SendPacket(udpClientSocket, serverTargetAddress, requestConnectionSegment);

    // Await for ack of sent segment
    AwaitAck();

    // If send buffer is still not empty return error:
    // failed to connect to server
    if (!sendBuffer.empty()) {
        return ERROR_CODE;
    }

	return 0;
}

void HandleConnectionRequest() {

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

// Note: weird quirk but only when sendBuffer is empty then the 
// function ends and then host can continue sending new packets
void AwaitAck() {

    std::cout << "Client is awaiting an acknowledgment of sent packets..." << std::endl;
    std::map<uint32_t, uint64_t> accumulative_timers = timers;

    while (true) {

        // Retransmission
        for (auto const [seqNum, timer] : timers) {
            uint64_t now = GetTimeNow();
            // The usual timeout
            if (now - timer >= TIMEOUT_MS && accumulative_timers[seqNum] <= TIMEOUT_MS_MAX) {
                if (SendPacket(udpClientSocket, serverTargetAddress, sendBuffer[seqNum])) {

                    accumulative_timers[seqNum] += (now - timer);
                }
            }
            else if (accumulative_timers[seqNum] > TIMEOUT_MS_MAX) {
                // Receiver unresponsive
                break;
            }

        }

        // Receive ACK
        struct sockaddr_in senderAddr;

        NetworkPacket recvUdpSegment = ReceivePacket(udpClientSocket, senderAddr);

        if (CompareSockaddr(senderAddr, serverTargetAddress)) {

            uint32_t seqNum = ntohl(recvUdpSegment.seqNumber);
            uint8_t flag = recvUdpSegment.flags;

            if (flag == 1) {

                // Mark packet as acknowledged is within window size
                if (sendBuffer.find(seqNum) != sendBuffer.end()) {
                    ackedPackets.insert(seqNum);

                    // Advance sendBase if possible
                    while (ackedPackets.count(sendBase)) {
                        std::cout << "Sliding window: removing " << sendBase << std::endl;
                        sendBuffer.erase(sendBase);
                        timers.erase(sendBase);
                        ackedPackets.erase(sendBase);
                        accumulative_timers.erase(sendBase);
                        sendBase = (sendBase + 1) % SEQ_NUM_SPACE;
                    }
                }
            }
        }

        // All sent packets within window were acknowledged
        if (sendBuffer.empty()) {
            break;
        }

    }
}

void SendAck() {

}

bool SendPacket(SOCKET socket, sockaddr_in address, NetworkPacket packet)
{
    if ((nextSeqNum % SEQ_NUM_SPACE == (sendBase + WIND_SIZE) % SEQ_NUM_SPACE)) {

        int sentBytes = sendto(socket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&address, sizeof(address));

        if (sentBytes == SOCKET_ERROR) {

            std::cerr << "Failed to send game data. Error: " << WSAGetLastError() << std::endl;
            return false;

        } else {

            // Update SR variables
            ++nextSeqNum;
            uint64_t now = GetTimeNow();
            sendBuffer[packet.seqNumber] = packet;
            timers[packet.seqNumber] = now;
            return true;

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
		std::cerr << "Failed to receive data. Error: " << WSAGetLastError() << std::endl;
	}

	return packet;
}

bool CompareSockaddr(const sockaddr_in& addr1, const sockaddr_in& addr2) {
    return (addr1.sin_family == addr2.sin_family &&
        addr1.sin_port == addr2.sin_port &&
        addr1.sin_addr.s_addr == addr2.sin_addr.s_addr);
}

void SendJoinRequest(SOCKET socket, sockaddr_in address) 
{
	/*NetworkPacket packet;
	packet.packetID = PacketID::JOIN_REQUEST;
	packet.sourcePortNumber = port;
	packet.destinationPortNumber = address.sin_port;
	SendPacket(socket, address, packet);*/
}

void HandleJoinRequest(SOCKET socket, sockaddr_in address, NetworkPacket packet)
{
	/*if (packet.packetID == PacketID::JOIN_REQUEST) 
	{
		std::cout << "Player [" << packet.sourcePortNumber << "] is joining the lobby." << std::endl;

		NetworkPacket responsePacket;
		responsePacket.packetID = PacketID::REQUEST_ACCEPTED;
		responsePacket.sourcePortNumber = port;
		responsePacket.destinationPortNumber = packet.sourcePortNumber;
		SendPacket(socket, address, responsePacket);
	}*/
}

void SendInput(SOCKET socket, sockaddr_in address) 
{
	/*NetworkPacket packet;
	packet.packetID = PacketID::GAME_INPUT;
	packet.sourcePortNumber = port;
	packet.destinationPortNumber = address.sin_port;
	strcpy_s(packet.data, "[PLAYER_INPUT_DATA]");
	SendPacket(socket, address, packet);*/
}

void HandlePlayerInput(SOCKET socket, sockaddr_in address, NetworkPacket packet)
{
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
	/*NetworkPacket packet;
	packet.packetID = PacketID::GAME_STATE_START;
	packet.sourcePortNumber = port;
	packet.destinationPortNumber = address.sin_port;
	SendPacket(socket, address, packet);*/
}

void ReceiveGameStateStart(SOCKET socket) 
{
	/*sockaddr_in address{};
	NetworkPacket packet = ReceivePacket(socket, address);

	if (packet.packetID == PacketID::GAME_STATE_START)
	{
		std::cout << "Game started. Initial game state: " << packet.data << std::endl;
	}*/
}

uint64_t GetTimeNow() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
}