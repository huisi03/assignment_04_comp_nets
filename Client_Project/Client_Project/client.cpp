/******************************************************************/
/*!
\file	client.cpp
\author Ng Yao Zhen, Eugene (n.yaozheneugene\@digipen.edu)
        Nivethavarsni D/O Ganapathi Arunachalam (nivethavarsni.d\@digipen.edu)
\par	Assignment 3
\date	19 Mar 2025
\brief
		This file implements the multi-threaded client file which mimics
        a server - client relationship that allows clients access to file
        downloading over UDP.

		Copyright (C) 2025 DigiPen Institute of Technology.
		Reproduction or disclosure of this file or its contents without the
		prior written consent of DigiPen Institute of Technology is prohibited.
*//****************************************************************/

/*******************************************************************************
 * A multi-threaded TCP/IP client with UDP file download capability
 ******************************************************************************/

/*                                                                      guard
----------------------------------------------------------------------------- */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS  

// WINSOCK
#define NOMINMAX
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>

// THREADING
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

// Other STL stuff
#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>                      
                 
// FILE AND I/O
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream> 

#pragma comment(lib, "ws2_32.lib")

// FOR Selective Repeat protocol
constexpr int WINDOW_SIZE = 30;                // Size of the sliding window
constexpr int SR_TIMEOUT_MS = 100;             // Timeout for retransmission (ms)

constexpr size_t BUFFER_SIZE = 4096;           // General buffer size for TCP
constexpr size_t UDP_BUFFER_SIZE = 8192;       // UDP buffer size
constexpr size_t MAX_UDP_PAYLOAD = 1400;       // Max UDP payload (to stay under typical MTU)
constexpr int MAX_RETRIES = 10;                // Maximum number of retransmission attempts
constexpr int ACK_TIMEOUT_MS = 500;            // Timeout for ACK in milliseconds

// Command IDs
enum CMDID {
    UNKNOWN = 0x0,         // Not in use
    REQ_QUIT = 0x1,
    REQ_DOWNLOAD = 0x2,
    RSP_DOWNLOAD = 0x3,
    REQ_LISTFILES = 0x4,
    RSP_LISTFILES = 0x5,
    CMD_TEST = 0x20,       // Not in use
    DOWNLOAD_ERROR = 0x30
};

// Structure for UDP packet header
#pragma pack(1)
struct UDPPacketHeader {
    uint8_t  flags;             // LSB: 0=DATA, 1=ACK
    uint32_t sessionId;         // Download session ID
    uint32_t fileOffset;        // Offset in the file for this chunk
    uint32_t dataLength;        // Length of data in this packet
};
#pragma pack()

// Global variables
std::mutex stdoutMutex;                  // Mutex for thread-safe console output
SOCKET tcpSocket = INVALID_SOCKET;       // TCP socket for control messages
SOCKET udpSocket = INVALID_SOCKET;       // UDP socket for file transfer
std::atomic<bool> running{ true };       // Controls client main loop
std::string clientPath;                  // Path to save downloaded files

// forward declarations
std::string getTimestamp();
void sendListFiles();
void sendDownloadRequest(const std::string& ip, uint16_t port, const std::string& filename);
void receiver();
bool receiveFileReliably(uint32_t sessionId, const std::string& filePath, uint32_t fileSize, const sockaddr_in& serverAddr);
void sendQuit();
void gracefulShutdown(SOCKET& socket);

/**
 * @brief Main entry point for the client application
 * @return Exit code
 */
int main() {
    // Detect script mode for automated testing
    bool scriptMode = !_isatty(_fileno(stdin));
    std::string serverIP, serverTcpPort, serverUdpPort, clientUdpPort;

    // Get server IP address
    if (!scriptMode) std::cout << "Server IP Address: ";
    std::getline(std::cin, serverIP);

    // Get server TCP port
    if (!scriptMode) std::cout << "Server TCP Port Number: ";
    std::getline(std::cin, serverTcpPort);

    // Get server UDP port
    if (!scriptMode) std::cout << "Server UDP Port Number: ";
    std::getline(std::cin, serverUdpPort);

    // Get client UDP port
    if (!scriptMode) std::cout << "Client UDP Port Number: ";
    std::getline(std::cin, clientUdpPort);

    // Get client path for storing downloaded files
    if (!scriptMode) std::cout << "Path: ";
    std::getline(std::cin, clientPath);

    // Create path if it doesn't exist
    if (!std::filesystem::exists(clientPath)) {
        try {
            std::filesystem::create_directories(clientPath);
        }
        catch (const std::exception& e) {
            std::cerr << "Error creating directory: " << e.what() << std::endl;
            return 1;
        }
    }

    // Initialize Winsock
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) {
        std::cerr << "WSAStartup() failed.\n";
        return 1;
    }

    // Set up address info for TCP connection
    addrinfo hints{}, * tcpInfo = nullptr;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;    // TCP
    hints.ai_protocol = IPPROTO_TCP;    // TCP protocol

    // Resolve the server address and port for TCP
    if (getaddrinfo(serverIP.c_str(), serverTcpPort.c_str(), &hints, &tcpInfo) != 0 || !tcpInfo) {
        std::cerr << "getaddrinfo() failed for TCP.\n";
        WSACleanup();
        return 2;
    }

    // Create TCP socket
    tcpSocket = socket(tcpInfo->ai_family, tcpInfo->ai_socktype, tcpInfo->ai_protocol);
    if (tcpSocket == INVALID_SOCKET) {
        std::cerr << "TCP socket creation failed.\n";
        freeaddrinfo(tcpInfo);
        WSACleanup();
        return 3;
    }

    // Connect to server via TCP
    if (connect(tcpSocket, tcpInfo->ai_addr, static_cast<int>(tcpInfo->ai_addrlen)) == SOCKET_ERROR) {
        std::cerr << "TCP connection failed.\n";
        freeaddrinfo(tcpInfo);
        closesocket(tcpSocket);
        WSACleanup();
        return 4;
    }

    freeaddrinfo(tcpInfo);

    // Create and bind UDP socket
    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        std::cerr << "UDP socket creation failed.\n";
        closesocket(tcpSocket);
        WSACleanup();
        return 5;
    }

    // Set up client's UDP address
    sockaddr_in clientUdpAddr{};
    clientUdpAddr.sin_family = AF_INET;
    clientUdpAddr.sin_addr.s_addr = INADDR_ANY;
    clientUdpAddr.sin_port = htons(static_cast<u_short>(std::stoi(clientUdpPort)));

    // Bind UDP socket
    if (bind(udpSocket, reinterpret_cast<sockaddr*>(&clientUdpAddr), sizeof(clientUdpAddr)) == SOCKET_ERROR) {
        std::cerr << "UDP bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(udpSocket);
        closesocket(tcpSocket);
        WSACleanup();
        return 6;
    }

    // Set both sockets to non-blocking mode
    u_long mode = 1;
    ioctlsocket(tcpSocket, FIONBIO, &mode);
    ioctlsocket(udpSocket, FIONBIO, &mode);

    // Get actual bound port
    sockaddr_in boundAddr{};
    int boundAddrLen = sizeof(boundAddr);
    getsockname(udpSocket, reinterpret_cast<sockaddr*>(&boundAddr), &boundAddrLen);
    uint16_t actualUdpPort = ntohs(boundAddr.sin_port);

    // Get client's actual IP address
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    addrinfo* localInfo = nullptr;
    getaddrinfo(hostname, nullptr, &hints, &localInfo);

    char clientIPAddr[INET_ADDRSTRLEN] = "127.0.0.1"; // Default to localhost
    for (addrinfo* ptr = localInfo; ptr != nullptr; ptr = ptr->ai_next) {
        if (ptr->ai_family == AF_INET) {
            sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
            inet_ntop(AF_INET, &(addr->sin_addr), clientIPAddr, INET_ADDRSTRLEN);
            break;
        }
    }

    freeaddrinfo(localInfo);

    // Display client information
    std::cout << "\nClient IP Address: " << clientIPAddr << std::endl;
    std::cout << "Client UDP Port: " << actualUdpPort << std::endl;
    std::cout << "Client Path: " << clientPath << std::endl << std::endl;
    std::cout << "Available commands:" << std::endl;
    std::cout << "  /l - List available files on server" << std::endl;
    std::cout << "  /d <ip>:<port> <filename> - Download file (using client's IP addr:port)" << std::endl;
    std::cout << "  /q - Quit the application." << std::endl << std::endl;

    // Start receiver thread for server responses
    std::thread recvThread(receiver);
    recvThread.detach(); // Let it run independently

    // Main loop for user input
    std::string input;
    while (running) {
        std::getline(std::cin, input);

        if (input.empty()) continue;

        if (input == "/q") {
            // Quit command
            sendQuit();
        }
        else if (input == "/l") {
            // List files command
            sendListFiles();
        }
        else if (input.rfind("/d ", 0) == 0) {
            // Download command
            std::string rest = input.substr(3); // Remove "/d " from the beginning
            size_t spacePos = rest.find(' ');

            if (spacePos == std::string::npos) {
                std::lock_guard<std::mutex> lock(stdoutMutex);
                std::cout << "Invalid format. Use: /d <ip>:<port> <filename>\n";
                continue;
            }

            std::string ipPort = rest.substr(0, spacePos); // IP:port part
            std::string filename = rest.substr(spacePos + 1); // Filename part

            size_t colonPos = ipPort.find(':');
            if (colonPos == std::string::npos) {
                std::lock_guard<std::mutex> lock(stdoutMutex);
                std::cout << "Invalid format. Use: /d <ip>:<port> <filename>\n";
                continue;
            }

            std::string ip = ipPort.substr(0, colonPos);
            std::string portStr = ipPort.substr(colonPos + 1);

            uint16_t port;
            try {
                port = static_cast<uint16_t>(std::stoi(portStr));
            }
            catch (...) {
                std::lock_guard<std::mutex> lock(stdoutMutex);
                std::cout << "Invalid port number\n";
                continue;
            }

            // Send download request
            sendDownloadRequest(ip, port, filename);
        }
        else {
            // Invalid command
            std::lock_guard<std::mutex> lock(stdoutMutex);
            std::cout << "Invalid command. Available commands: /l, /d, /q\n";
        }
    }

    // Clean up
    gracefulShutdown(tcpSocket);
    gracefulShutdown(udpSocket);
    WSACleanup();

    return 0;
}

/**
 * @brief Sends download request for a file
 * @param ip Client's IP address
 * @param port Client's UDP port
 * @param filename Name of file to download
 */
void sendDownloadRequest(const std::string& ip, uint16_t port, const std::string& filename) {
    // Convert IP to binary form
    in_addr clientIP{};
    if (inet_pton(AF_INET, ip.c_str(), &clientIP) <= 0) {
        std::lock_guard<std::mutex> lock(stdoutMutex);
        std::cerr << "Invalid IP address: " << ip << std::endl;
        return;
    }

    // Create download request packet
    std::vector<uint8_t> packet(11 + filename.size());
    packet[0] = REQ_DOWNLOAD;
    *reinterpret_cast<uint32_t*>(&packet[1]) = clientIP.s_addr;
    *reinterpret_cast<uint16_t*>(&packet[5]) = htons(port);
    *reinterpret_cast<uint32_t*>(&packet[7]) = htonl(static_cast<uint32_t>(filename.size()));
    memcpy(&packet[11], filename.c_str(), filename.size());

    // Send packet
    send(tcpSocket, reinterpret_cast<char*>(packet.data()), static_cast<int>(packet.size()), 0);

    {
        std::lock_guard<std::mutex> lock(stdoutMutex);
        std::cout << "[" << getTimestamp() << "] Requesting download of '" << filename
            << "' via " << ip << ":" << port << "\n";
    }
}

/**
 * @brief Receives file reliably over UDP using stop-and-wait protocol
 * @param sessionId Unique session identifier
 * @param filePath Path to save the file
 * @param fileSize Expected size of the file
 * @param serverAddr Server address structure
 * @return true if file was received successfully
 */
bool receiveFileReliably(uint32_t sessionId,
    const std::string& filePath,
    uint32_t fileSize,
    const sockaddr_in& serverAddr)
{
    // Calculate number of packets expected
    uint32_t totalPackets = (fileSize + MAX_UDP_PAYLOAD - 1) / MAX_UDP_PAYLOAD;

    {
        std::lock_guard<std::mutex> lock(stdoutMutex);
        std::cout << "  Starting file transfer with sessionId=" << sessionId
            << ", expectedPackets=" << totalPackets << std::endl;
    }

    // Open output file
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        std::lock_guard<std::mutex> lock(stdoutMutex);
        std::cerr << "  Error: Cannot create file: " << filePath << std::endl;
        return false;
    }

    // Pre-allocate file size
    try {
        file.seekp(fileSize - 1);
        file.put(0); // Write a byte to ensure the file is properly sized
        file.seekp(0);
    }
    catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stdoutMutex);
        std::cerr << "  Error pre-allocating file: " << e.what() << std::endl;
        return false;
    }

    // Selective Repeat state variables
    uint32_t rcvBase = 0;                // Base of receive window

    // Track which packets have been received
    std::vector<bool> received(totalPackets, false);

    // Buffer for out-of-order packets
    struct PacketBuffer {
        std::vector<char> data;
        uint32_t length;
    };
    std::vector<PacketBuffer> packetBuffers(totalPackets);

    // Statistics
    uint32_t receivedPackets = 0;
    uint32_t outOfOrderPackets = 0;
    uint32_t totalBytes = 0;

    // Transfer timing
    auto startTime = std::chrono::steady_clock::now();

    // Main receive loop
    bool done = false;
    while (!done && running) {
        // Wait for data with timeout
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(udpSocket, &readSet);

        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000; // 200ms timeout

        int sel = select(udpSocket + 1, &readSet, nullptr, nullptr, &tv);

        if (sel > 0) {
            // Data available to read
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            // Buffer to hold UDP packet
            std::vector<char> buffer(sizeof(UDPPacketHeader) + MAX_UDP_PAYLOAD);

            int bytesReceived = recvfrom(udpSocket,
                buffer.data(),
                static_cast<int>(buffer.size()),
                0,
                reinterpret_cast<sockaddr*>(&fromAddr),
                &fromLen);

            // Process packet if it's valid
            if (bytesReceived >= static_cast<int>(sizeof(UDPPacketHeader))) {
                // Extract header fields
                auto* hdr = reinterpret_cast<UDPPacketHeader*>(buffer.data());
                uint8_t flags = hdr->flags;
                uint32_t pktSessionId = ntohl(hdr->sessionId);
                uint32_t seqNum = ntohl(hdr->fileOffset);
                uint32_t dataLength = ntohl(hdr->dataLength);

                // Process only data packets from our session
                if (flags == 0x00 && pktSessionId == sessionId && seqNum < totalPackets) {
                    // Always send ACK for received packets (key difference from Go-Back-N)
                    char ackPacket[9] = { 0 };
                    ackPacket[0] = 0x01;  // ACK flag
                    *reinterpret_cast<uint32_t*>(&ackPacket[1]) = htonl(sessionId);
                    *reinterpret_cast<uint32_t*>(&ackPacket[5]) = htonl(seqNum);

                    sendto(udpSocket,
                        ackPacket,
                        sizeof(ackPacket),
                        0,
                        reinterpret_cast<const sockaddr*>(&serverAddr),
                        sizeof(serverAddr));

                    // Process packet data if we haven't received this packet before
                    if (!received[seqNum]) {
                        received[seqNum] = true;
                        receivedPackets++;
                        totalBytes += dataLength;

                        // Check if this is in-order (at rcvBase) or out-of-order
                        if (seqNum == rcvBase) {
                            // In-order packet - write directly to file
                            file.seekp(seqNum * MAX_UDP_PAYLOAD);
                            file.write(buffer.data() + sizeof(UDPPacketHeader), dataLength);

                            // Move receive window forward, checking for already received packets
                            rcvBase++;

                            // Write any buffered packets that are now in order
                            while (rcvBase < totalPackets && received[rcvBase]) {
                                // We have this packet buffered - write it
                                if (packetBuffers[rcvBase].data.size() > 0) {
                                    file.seekp(rcvBase * MAX_UDP_PAYLOAD);
                                    file.write(packetBuffers[rcvBase].data.data(),
                                        packetBuffers[rcvBase].length);

                                    // Clear buffer to save memory
                                    std::vector<char>().swap(packetBuffers[rcvBase].data);
                                }
                                rcvBase++;
                            }
                        }
                        else {
                            // Out-of-order packet - buffer for later
                            outOfOrderPackets++;

                            // Store in our buffer
                            packetBuffers[seqNum].data.resize(dataLength);
                            packetBuffers[seqNum].length = dataLength;

                            // Copy data portion
                            memcpy(packetBuffers[seqNum].data.data(),
                                buffer.data() + sizeof(UDPPacketHeader),
                                dataLength);
                        }

                        // Log progress every 100 packets
                        if (receivedPackets % 100 == 0) {
                            std::lock_guard<std::mutex> lock(stdoutMutex);
                            std::cout << "  Progress: " << receivedPackets << "/" << totalPackets
                                << " packets (" << (receivedPackets * 100 / totalPackets) << "%)" << std::endl;
                        }
                    }
                }
            }
        }

        // Check if we're done
        if (receivedPackets >= totalPackets || rcvBase >= totalPackets) {
            done = true;
        }

        // Timeout check - abort if taking too long
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

        // If transfer takes more than 5 minutes, consider it failed
        if (elapsed > 300) {
            std::lock_guard<std::mutex> lock(stdoutMutex);
            std::cerr << "  Transfer timeout after " << elapsed << " seconds." << std::endl;
            return false;
        }
    }

    // Make sure file is properly flushed and closed
    file.flush();
    file.close();

    // Calculate transfer statistics
    auto endTime = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() / 1000.0;
    double speedMBps = (totalBytes / (1024.0 * 1024.0)) / elapsedSec;

    // Log completion
    {
        std::lock_guard<std::mutex> lock(stdoutMutex);
        std::cout << "  Transfer complete! Session ID:" << sessionId << std::endl;
        std::cout << "  Statistics:" << std::endl;
        std::cout << "  Total packets: " << totalPackets << std::endl;
        std::cout << "  Received packets: " << receivedPackets << std::endl;
        std::cout << "  Out-of-order packets: " << outOfOrderPackets << std::endl;
        std::cout << "  Transfer rate: " << std::fixed << std::setprecision(2) << speedMBps << " MB/s" << std::endl;
        std::cout << "  Time: " << elapsedSec << " seconds" << std::endl;
    }

    return true;
}

/**
 * @brief Thread function for handling incoming messages from server
 */
void receiver() {
    std::vector<uint8_t> buffer;
    char temp[BUFFER_SIZE];

    while (running) {
        int received = recv(tcpSocket, temp, BUFFER_SIZE, 0);

        if (received == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            break;
        }

        if (received <= 0) {
            std::lock_guard<std::mutex> lock(stdoutMutex);
            std::cout << "Connection closed by server.\n";
            running = false;
            break;
        }

        buffer.insert(buffer.end(), temp, temp + received);

        while (!buffer.empty()) {
            uint8_t cmd = buffer[0];
            bool handled = true;

            try {
                switch (cmd) {
                case RSP_LISTFILES: {
                    if (buffer.size() < 3) { // CMD(1) + FileCount(2)
                        handled = false;
                        break;
                    }

                    uint16_t fileCount = ntohs(*reinterpret_cast<uint16_t*>(&buffer[1]));

                    std::lock_guard<std::mutex> lock(stdoutMutex);
                    std::cout << "\n==========FILES START==========\n";
                    std::cout << "Available files (" << fileCount << "):\n";

                    size_t offset = 3;
                    bool complete = true;

                    for (uint16_t i = 0; i < fileCount; ++i) {
                        if (offset + 4 > buffer.size()) {
                            complete = false;
                            break;
                        }

                        uint32_t filenameLen = ntohl(*reinterpret_cast<uint32_t*>(&buffer[offset]));
                        offset += 4;

                        if (offset + filenameLen > buffer.size()) {
                            complete = false;
                            break;
                        }

                        std::string filename(reinterpret_cast<char*>(&buffer[offset]), filenameLen);
                        offset += filenameLen;

                        std::cout << "  " << filename << "\n";
                    }

                    if (complete) {
                        std::cout << "==========FILES END==========\n";
                        buffer.erase(buffer.begin(), buffer.begin() + offset);
                    }
                    else {
                        handled = false; // Wait for more data
                    }
                    break;
                }

                case RSP_DOWNLOAD: {
                    if (buffer.size() < 15) { // CMD(1) + IP(4) + PORT(2) + SessionID(4) + FileLen(4)
                        handled = false;
                        break;
                    }

                    uint32_t serverIP = *reinterpret_cast<uint32_t*>(&buffer[1]);
                    uint16_t serverPort = ntohs(*reinterpret_cast<uint16_t*>(&buffer[5]));
                    uint32_t sessionID = ntohl(*reinterpret_cast<uint32_t*>(&buffer[7]));
                    uint32_t fileSize = ntohl(*reinterpret_cast<uint32_t*>(&buffer[11]));

                    std::string filename;
                    if (buffer.size() >= 19) { // Extra 4 bytes for filename length
                        uint32_t filenameLen = ntohl(*reinterpret_cast<uint32_t*>(&buffer[15]));
                        if (buffer.size() >= 19 + filenameLen) {
                            filename = std::string(reinterpret_cast<char*>(&buffer[19]), filenameLen);
                            buffer.erase(buffer.begin(), buffer.begin() + 19 + filenameLen);
                        }
                        else {
                            handled = false;
                            break;
                        }
                    }
                    else {
                        // No filename provided, use default name
                        filename = "download_" + std::to_string(sessionID);
                        buffer.erase(buffer.begin(), buffer.begin() + 15);
                    }

                    // Convert IP to string
                    in_addr addr{};
                    addr.s_addr = serverIP;
                    char ipStr[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &addr, ipStr, INET_ADDRSTRLEN);

                    {
                        std::lock_guard<std::mutex> lock(stdoutMutex);
                        std::cout << "\n==========DOWNLOAD INFO==========\n";
                        std::cout << "Server: " << ipStr << ":" << serverPort << "\n";
                        std::cout << "Session ID: " << sessionID << "\n";
                        std::cout << "File size: " << fileSize << " bytes\n";
                        std::cout << "Filename: " << filename << "\n";
                        std::cout << "Using: Selective Repeat Protocol (window=" << WINDOW_SIZE << ")\n";
                        std::cout << "=================================\n";
                    }

                    // Setup server address for UDP communication
                    sockaddr_in serverAddr{};
                    serverAddr.sin_family = AF_INET;
                    serverAddr.sin_addr.s_addr = serverIP;
                    serverAddr.sin_port = htons(serverPort);

                    // Generate output filename
                    std::string outputPath = clientPath + "\\" + filename;

                    // Start receiver thread
                    std::thread downloadThread([=]() {
                        bool success = receiveFileReliably(sessionID, outputPath, fileSize, serverAddr);

                        std::lock_guard<std::mutex> lock(stdoutMutex);
                        if (success) {
                            std::cout << "\n==========DOWNLOAD COMPLETE==========\n";
                            std::cout << "File saved to: " << outputPath << "\n";
                            std::cout << "=====================================\n";
                        }
                        else {
                            std::cout << "\n==========DOWNLOAD FAILED==========\n";
                            std::cout << "Session ID: " << sessionID << "\n";
                            std::cout << "===================================\n";
                        }
                        });
                    downloadThread.detach();
                    break;
                }

                case DOWNLOAD_ERROR: {
                    std::lock_guard<std::mutex> lock(stdoutMutex);
                    std::cout << "\n==========ERROR==========\n";
                    std::cout << "Download error: File not found or server error\n";
                    std::cout << "=========================\n";

                    buffer.erase(buffer.begin());
                    break;
                }

                default: {
                    // Unknown command, just remove it
                    buffer.erase(buffer.begin());
                    break;
                }
                }
            }
            catch (...) {
                // If any exception occurs, mark as not handled
                handled = false;
            }

            if (!handled) break;  // Wait for more data if packet wasn't fully handled
        }
    }
}

/**
 * @brief Sends a quit command to server
 */
void sendQuit() {
    uint8_t cmd = REQ_QUIT;
    send(tcpSocket, reinterpret_cast<char*>(&cmd), 1, 0);
    running = false;
}

/**
 * @brief Sends a request for file listing to server
 */
void sendListFiles() {
    uint8_t cmd = REQ_LISTFILES;
    send(tcpSocket, reinterpret_cast<char*>(&cmd), 1, 0);

    std::lock_guard<std::mutex> lock(stdoutMutex);
    std::cout << "[" << getTimestamp() << "] Requesting file list...\n";
}

/**
 * @brief Gets current time as formatted string
 * @return Formatted timestamp string
 */
std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%H:%M:%S");
    return ss.str();
}

/**
 * @brief Performs graceful shutdown of a socket
 * @param socket Socket to shutdown
 */
void gracefulShutdown(SOCKET& socket) {
    if (socket != INVALID_SOCKET) {
        shutdown(socket, SD_BOTH);      // Shutdown both send and receive
        closesocket(socket);            // Close the socket
        socket = INVALID_SOCKET;        // Invalidate the handle
    }
}