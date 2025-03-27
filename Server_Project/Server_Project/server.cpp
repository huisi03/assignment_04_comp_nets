/******************************************************************/
/*!
\file	server.cpp
\author Ng Yao Zhen, Eugene (n.yaozheneugene\@digipen.edu)
        Nivethavarsni D/O Ganapathi Arunachalam (nivethavarsni.d\@digipen.edu)
\par	Assignment 3
\date	19 Mar 2025
\brief
		This file implements the server file which will be used to
        implement a server for establishing connections over TCP with
        multiple clients, as well as a connection-less UDP state that
        handles clients' request(s) to download files.

		Copyright (C) 2025 DigiPen Institute of Technology.
		Reproduction or disclosure of this file or its contents without the
		prior written consent of DigiPen Institute of Technology is prohibited.
*//****************************************************************/

/*******************************************************************************
 * A multi-threaded TCP/IP server with UDP file transfer capability
 ******************************************************************************/

/*                                                                      guard
----------------------------------------------------------------------------- */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WINSOCK_DEPRECATED_NO_WARNINGS  // Disable deprecation warnings for older Winsock functions  
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
#include <thread>
#include <mutex>

// Other STL stuff
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

// FILE and I/O
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>

#include "taskqueue.h"

#pragma comment(lib, "ws2_32.lib")

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

// Constants
constexpr size_t BUFFER_SIZE     = 4096;       // General buffer size
constexpr int MAX_RETRIES        = 10;         // Maximum number of retransmission attempts

// Static globals
static const size_t MAX_UDP_PAYLOAD = 1400;    // Max UDP payload (to stay under typical MTU)
static const int WINDOW_SIZE = 30;             // Number of packets "in flight"
static const int SR_TIMEOUT_MS = 100;          // Timeout (ms) per packet before retransmission
static const int ACK_POLLING_MS = 5;           // Polling delay for ACKs

// Structure for UDP packet header
#pragma pack(1)
struct UDPPacketHeader {
    uint8_t  flags;             // LSB: 0=DATA, 1=ACK
    uint32_t sessionId;         // Download session ID
    uint32_t fileOffset;        // Offset in the file for this chunk
    uint32_t dataLength;        // Length of data in this packet
};
#pragma pack()

std::string serverPath;                             // Path to files for downloading
SOCKET tcpListener = INVALID_SOCKET;                // UDP socket for file transfers

// Mutex for thread-safe console output
std::mutex stdoutMutex;
std::mutex transfersMutex;                          // Mutex for access to activeTransfers
std::atomic<bool> serverRunning{ true };            // Controls server main loop
std::atomic<uint32_t> nextSessionId{ 1 };           // Counter for generating unique session IDs
std::map<std::pair<uint32_t, uint16_t>, SOCKET> connectedClients;  // Maps client IP:Port to socket
std::map<uint32_t, std::thread> activeTransfers;    // Map of active file transfer threads

// forward declarations
bool execute(SOCKET clientSocket);
std::vector<std::string> listFiles(const std::string& path);
bool sendFileReliably(uint32_t sessionId, const std::string& filePath, const sockaddr_in& clientAddr, SOCKET transferSocket);
void gracefulShutdown(SOCKET& socket);

int main() {
    bool scriptMode = !_isatty(_fileno(stdin));

    std::string sTcpPort, sUdpPort;
    if (!scriptMode) std::cout << "Server TCP Port Number: ";
    std::getline(std::cin, sTcpPort);

    if (!scriptMode) std::cout << "Server UDP Port Number: ";
    std::getline(std::cin, sUdpPort);

    if (!scriptMode) std::cout << "Path: ";
    std::getline(std::cin, serverPath);

    // Init Winsock
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup() failed.\n";
        return 1;
    }

    // Setup TCP listener
    addrinfo hints{}, * tcpInfo = nullptr;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(nullptr, sTcpPort.c_str(), &hints, &tcpInfo) != 0 || !tcpInfo) {
        std::cerr << "getaddrinfo() failed for TCP.\n";
        WSACleanup();
        return 2;
    }
    tcpListener = socket(tcpInfo->ai_family, tcpInfo->ai_socktype, tcpInfo->ai_protocol);
    if (tcpListener == INVALID_SOCKET) {
        std::cerr << "TCP socket() failed.\n";
        freeaddrinfo(tcpInfo);
        WSACleanup();
        return 3;
    }
    if (bind(tcpListener, tcpInfo->ai_addr, (int)tcpInfo->ai_addrlen) == SOCKET_ERROR) {
        std::cerr << "TCP bind() failed.\n";
        freeaddrinfo(tcpInfo);
        closesocket(tcpListener);
        WSACleanup();
        return 4;
    }
    freeaddrinfo(tcpInfo);
    if (listen(tcpListener, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed.\n";
        closesocket(tcpListener);
        WSACleanup();
        return 5;
    }

    // Setup the single UDP socket if needed for control messages (not used for file transfers)
    SOCKET globalUdpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (globalUdpSocket == INVALID_SOCKET) {
        std::cerr << "UDP socket creation failed.\n";
        closesocket(tcpListener);
        WSACleanup();
        return 6;
    }
    sockaddr_in udpAddr{};
    udpAddr.sin_family = AF_INET;
    udpAddr.sin_addr.s_addr = INADDR_ANY;
    udpAddr.sin_port = htons(static_cast<u_short>(std::stoi(sUdpPort)));
    if (bind(globalUdpSocket, reinterpret_cast<sockaddr*>(&udpAddr), sizeof(udpAddr)) == SOCKET_ERROR) {
        std::cerr << "UDP bind() failed.\n";
        closesocket(globalUdpSocket);
        closesocket(tcpListener);
        WSACleanup();
        return 7;
    }

    // Set non-blocking mode for the TCP listener and global UDP socket
    u_long mode = 1;
    ioctlsocket(tcpListener, FIONBIO, &mode);
    ioctlsocket(globalUdpSocket, FIONBIO, &mode);

    // Get server IP for display
    char hostname[256];
    char serverIPAddr[INET_ADDRSTRLEN] = "127.0.0.1";
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        addrinfo* localInfo = nullptr;
        if (getaddrinfo(hostname, nullptr, &hints, &localInfo) == 0 && localInfo) {
            for (auto* ptr = localInfo; ptr; ptr = ptr->ai_next) {
                if (ptr->ai_family == AF_INET) {
                    auto* sinp = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
                    inet_ntop(AF_INET, &sinp->sin_addr, serverIPAddr, INET_ADDRSTRLEN);
                    break;
                }
            }
            freeaddrinfo(localInfo);
        }
    }

    // Ensure server path exists
    if (!std::filesystem::exists(serverPath)) {
        std::cerr << "Warning: server path does not exist. Creating it.\n";
        std::filesystem::create_directories(serverPath);
    }

    std::cout << "\nServer IP Address: " << serverIPAddr << std::endl;
    std::cout << "Server TCP Port Number: " << sTcpPort << std::endl;
    std::cout << "Server UDP Port Number: " << sUdpPort << std::endl;
    std::cout << "Server Path: " << serverPath << std::endl << std::endl;
    std::cout << "Selective Repeat Protocol: Window Size = " << WINDOW_SIZE
        << ", Timeout = " << SR_TIMEOUT_MS << "ms" << std::endl;

    // Cleanup callback for TaskQueue
    auto onDisconnect = [&]() {
        gracefulShutdown(tcpListener);
        gracefulShutdown(globalUdpSocket);
        WSACleanup();
        };

    // Create a TaskQueue with 10 to 20 worker threads
    TaskQueue<SOCKET, decltype(execute), decltype(onDisconnect)>
        tq(10, 20, execute, onDisconnect);

    // Accept loop
    while (serverRunning) {
        sockaddr_in clientAddress{};
        int addrSize = sizeof(clientAddress);

        SOCKET client = accept(tcpListener,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &addrSize);
        if (client == INVALID_SOCKET) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                std::cerr << "accept() failed, err = " << err << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Non-blocking
        ioctlsocket(client, FIONBIO, &mode);

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, INET_ADDRSTRLEN);
        uint16_t cport = ntohs(clientAddress.sin_port);
        uint32_t ip = ntohl(clientAddress.sin_addr.s_addr);

        {
            std::lock_guard<std::mutex> lock(stdoutMutex);
            std::cout << "\nClient Connected - IP: "
                << clientIP << " Port: " << cport << std::endl;
            connectedClients[{ip, cport}] = client;
        }

        // Hand off to a worker thread
        tq.produce(client);
    }

    // Wait for any active transfers to finish
    {
        std::lock_guard<std::mutex> lock(transfersMutex);
        for (auto& [sid, t] : activeTransfers) {
            if (t.joinable()) t.join();
        }
        activeTransfers.clear();
    }

    // Final cleanup
    gracefulShutdown(tcpListener);
    gracefulShutdown(globalUdpSocket);
    WSACleanup();
    return 0;
}

/**
 * @brief Lists all files in the specified directory
 * @param path Directory path to list files from
 * @return Vector of filenames
 */
std::vector<std::string> listFiles(const std::string& path) {
    std::vector<std::string> files;
    try {
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (!std::filesystem::is_directory(entry.path())) {
                files.push_back(entry.path().filename().string());
            }
        }
    }
    catch (const std::exception& e) {
        std::lock_guard<std::mutex> lk(stdoutMutex);
        std::cerr << "Error listing files: " << e.what() << std::endl;
    }
    return files;
}

/**
 * @brief Sends a file reliably over UDP using stop-and-wait protocol
 * @param sessionId Unique session identifier
 * @param filePath Path to the file to send
 * @param clientAddr Client address structure
 * @return true if file was sent successfully
 */
bool sendFileReliably(uint32_t sessionId, const std::string& filePath, const sockaddr_in& clientAddr, SOCKET transferSocket) {
    // Open and read file
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::lock_guard<std::mutex> lk(stdoutMutex);
        std::cerr << "Error: Cannot open file: " << filePath << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    uint32_t fileSize = static_cast<uint32_t>(file.tellg());
    file.seekg(0);

    // Read entire file into memory
    std::vector<char> fileData(fileSize);
    file.read(fileData.data(), fileSize);
    file.close();

    // Create packets from file data
    struct Packet {
        std::vector<char> data;           // Complete packet (header + payload)
        uint32_t seqNum;                  // Sequence number (= file offset index)
        uint32_t length;                  // Length of payload
        bool acked;                       // Acknowledgment state
        std::chrono::steady_clock::time_point lastSent; // Last sent time
        int retries;                      // Retries counter
    };

    // Determine total number of packets
    uint32_t totalPackets = (fileSize + MAX_UDP_PAYLOAD - 1) / MAX_UDP_PAYLOAD;
    std::vector<Packet> packets;
    packets.reserve(totalPackets);

    {
        std::lock_guard<std::mutex> lk(stdoutMutex);
        std::cout << "Starting transfer for Session ID: " << sessionId
            << ", totalPackets=" << totalPackets
            << ", fileSize=" << fileSize << " bytes" << std::endl;
    }

    // Prepare packet array with header and payload
    for (uint32_t seq = 0; seq < totalPackets; seq++) {
        uint32_t offset = seq * MAX_UDP_PAYLOAD;
        uint32_t chunkSize = std::min<uint32_t>(MAX_UDP_PAYLOAD, fileSize - offset);

        std::vector<char> packetData(sizeof(UDPPacketHeader) + chunkSize);
        auto* hdr = reinterpret_cast<UDPPacketHeader*>(packetData.data());
        hdr->flags = 0x00; // data packet
        hdr->sessionId = htonl(sessionId);
        hdr->fileOffset = htonl(seq);
        hdr->dataLength = htonl(chunkSize);

        memcpy(packetData.data() + sizeof(UDPPacketHeader),
            fileData.data() + offset,
            chunkSize);

        Packet p;
        p.data = std::move(packetData);
        p.seqNum = seq;
        p.length = chunkSize;
        p.acked = false;
        p.retries = 0;
        packets.push_back(std::move(p));
    }

    // Selective Repeat state variables
    uint32_t base = 0;              // first unacked packet
    uint32_t nextSeqNum = 0;        // next packet index to send
    bool transferComplete = false;
    std::unordered_set<uint32_t> unackedPackets;  // set of sequence numbers unacknowledged

    auto startTime = std::chrono::steady_clock::now();

    while (!transferComplete && serverRunning) {
        // 1. Send new packets (within the window)
        while (nextSeqNum < totalPackets && nextSeqNum < base + WINDOW_SIZE) {
            // Send packet using our dedicated transferSocket (no global locking needed)
            sendto(transferSocket,
                packets[nextSeqNum].data.data(),
                static_cast<int>(packets[nextSeqNum].data.size()),
                0,
                reinterpret_cast<const sockaddr*>(&clientAddr),
                sizeof(clientAddr));

            packets[nextSeqNum].lastSent = std::chrono::steady_clock::now();
            unackedPackets.insert(nextSeqNum);
            nextSeqNum++;
        }

        // 2. Check for incoming ACKs (using a select with a short timeout)
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(transferSocket, &readSet);

        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = ACK_POLLING_MS * 1000; // use ACK_POLLING_MS milliseconds

        int sel = select(static_cast<int>(transferSocket + 1), &readSet, nullptr, nullptr, &tv);
        if (sel > 0) {
            char ackBuf[9];  // 9 bytes: flags (1) + sessionId (4) + seqNum (4)
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);
            int bytesRead = recvfrom(transferSocket,
                ackBuf,
                sizeof(ackBuf),
                0,
                reinterpret_cast<sockaddr*>(&fromAddr),
                &fromLen);

            if (bytesRead == 9 && ackBuf[0] == 0x01) {
                uint32_t ackSession = ntohl(*reinterpret_cast<uint32_t*>(&ackBuf[1]));
                uint32_t ackSeq = ntohl(*reinterpret_cast<uint32_t*>(&ackBuf[5]));

                if (ackSession == sessionId && ackSeq < totalPackets) {
                    if (!packets[ackSeq].acked) {
                        packets[ackSeq].acked = true;
                        unackedPackets.erase(ackSeq);
                        if (ackSeq == base) {
                            while (base < totalPackets && packets[base].acked)
                                base++;
                        }
                    }
                }
            }
        }

        // 3. Check timeouts on unacknowledged packets and retransmit if necessary
        auto now = std::chrono::steady_clock::now();
        for (uint32_t seq : unackedPackets) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - packets[seq].lastSent).count();
            if (elapsed > SR_TIMEOUT_MS) {
                // Retransmit packet
                {
                    std::lock_guard<std::mutex> lk(stdoutMutex);
                    std::cout << "Timeout => Resending packet seq=" << seq << std::endl;
                }
                sendto(transferSocket,
                    packets[seq].data.data(),
                    static_cast<int>(packets[seq].data.size()),
                    0,
                    reinterpret_cast<const sockaddr*>(&clientAddr),
                    sizeof(clientAddr));
                packets[seq].lastSent = now;
                packets[seq].retries++;
                if (packets[seq].retries > MAX_RETRIES) {
                    std::lock_guard<std::mutex> lk(stdoutMutex);
                    std::cerr << "Error: Maximum retries exceeded for packet " << seq << std::endl;
                    return false;
                }
            }
        }

        if (base >= totalPackets) {
            transferComplete = true;
        }
    }

    // Calculate and show transfer statistics
    int totalRetransmits = 0;
    for (const auto& packet : packets)
        totalRetransmits += packet.retries;
    auto endTime = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() / 1000.0;
    {
        std::lock_guard<std::mutex> lk(stdoutMutex);
        std::cout << "Transfer complete! Session ID: " << sessionId
            << ", packets=" << totalPackets
            << ", retransmits=" << totalRetransmits << std::endl;
        std::cout << "Time: " << elapsedSec << " seconds" << std::endl;
    }
    return true;
}

/**
 * @brief Handles client communication
 * @param client Client socket to handle
 * @return true if client was handled successfully
 *
 * This function is called by task queue threads to handle client connections.
 * It processes incoming messages and routes them appropriately.
 */
bool execute(SOCKET client) {
    // Get client address information
    sockaddr_in clientAddr{};
    int addrLen = sizeof(clientAddr);
    if (getpeername(client, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen) != 0) {
        closesocket(client);
        return false;
    }

    uint32_t ipHost = ntohl(clientAddr.sin_addr.S_un.S_addr);
    uint16_t port = ntohs(clientAddr.sin_port);
    std::vector<uint8_t> buffer;
    char temp[BUFFER_SIZE];

    // Client communication loop
    while (serverRunning) {
        int received = recv(client, temp, BUFFER_SIZE, 0);
        if (received == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            break;
        }
        if (received <= 0) break;
        buffer.insert(buffer.end(), temp, temp + received);

        while (!buffer.empty()) {
            uint8_t cmd = buffer[0];
            bool valid = true;
            try {
                switch (cmd) {
                case REQ_QUIT: {
                    // Client wants to disconnect
                    buffer.clear();
                    goto disconnect;
                }
                case REQ_LISTFILES: {
                    // Build file list response
                    auto files = listFiles(serverPath);
                    size_t totalSize = 3;
                    for (auto& f : files)
                        totalSize += 4 + f.size();
                    std::vector<uint8_t> response(totalSize);
                    response[0] = RSP_LISTFILES;
                    *reinterpret_cast<uint16_t*>(&response[1]) =
                        htons(static_cast<uint16_t>(files.size()));
                    size_t offset = 3;
                    for (auto& f : files) {
                        *reinterpret_cast<uint32_t*>(&response[offset]) =
                            htonl(static_cast<uint32_t>(f.size()));
                        offset += 4;
                        memcpy(&response[offset], f.data(), f.size());
                        offset += f.size();
                    }
                    size_t sent = 0;
                    while (sent < response.size()) {
                        int n = send(client,
                            reinterpret_cast<const char*>(response.data()) + sent,
                            static_cast<int>(response.size() - sent),
                            0);
                        if (n <= 0) break;
                        sent += n;
                    }
                    buffer.erase(buffer.begin());
                    break;
                }
                case REQ_DOWNLOAD: {
                    // Expected format:
                    // CMD(1) + Client IP(4) + Client Port(2) + FilenameLen(4) + Filename
                    if (buffer.size() < 11) {
                        valid = false;
                        break;
                    }
                    uint32_t clientIP = *reinterpret_cast<uint32_t*>(&buffer[1]);
                    uint16_t clientPort = ntohs(*reinterpret_cast<uint16_t*>(&buffer[5]));
                    uint32_t filenameLen = ntohl(*reinterpret_cast<uint32_t*>(&buffer[7]));
                    if (buffer.size() < 11 + filenameLen) {
                        valid = false;
                        break;
                    }
                    std::string filename(reinterpret_cast<char*>(&buffer[11]), filenameLen);
                    buffer.erase(buffer.begin(), buffer.begin() + 11 + filenameLen);
                    {
                        std::lock_guard<std::mutex> lock(stdoutMutex);
                        std::cout << "\nDownload request for file: " << filename << std::endl;
                    }
                    std::string fullPath = serverPath + "/" + filename;
                    std::ifstream fileCheck(fullPath, std::ios::binary);
                    if (!fileCheck) {
                        uint8_t errorCmd = DOWNLOAD_ERROR;
                        send(client, reinterpret_cast<char*>(&errorCmd), 1, 0);
                        break;
                    }
                    fileCheck.seekg(0, std::ios::end);
                    uint32_t fileSize = static_cast<uint32_t>(fileCheck.tellg());
                    fileCheck.close();

                    uint32_t sessionId = nextSessionId++;

                    // Create a dedicated UDP socket for this transfer.
                    SOCKET transferSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    if (transferSocket == INVALID_SOCKET) {
                        uint8_t errorCmd = DOWNLOAD_ERROR;
                        send(client, reinterpret_cast<char*>(&errorCmd), 1, 0);
                        break;
                    }
                    sockaddr_in transferAddr{};
                    transferAddr.sin_family = AF_INET;
                    transferAddr.sin_addr.s_addr = INADDR_ANY;
                    transferAddr.sin_port = 0; // Let the system choose an ephemeral port
                    if (bind(transferSocket, reinterpret_cast<sockaddr*>(&transferAddr), sizeof(transferAddr)) == SOCKET_ERROR) {
                        closesocket(transferSocket);
                        uint8_t errorCmd = DOWNLOAD_ERROR;
                        send(client, reinterpret_cast<char*>(&errorCmd), 1, 0);
                        break;
                    }
                    int transferAddrLen = sizeof(transferAddr);
                    getsockname(transferSocket, reinterpret_cast<sockaddr*>(&transferAddr), &transferAddrLen);

                    // Get server IP (the same way as before)
                    char hostname[256];
                    char serverIPAddr[INET_ADDRSTRLEN] = "127.0.0.1";
                    if (gethostname(hostname, sizeof(hostname)) == 0) {
                        addrinfo hints{}, * localInfo = nullptr;
                        ZeroMemory(&hints, sizeof(hints));
                        hints.ai_family = AF_INET;
                        if (getaddrinfo(hostname, NULL, &hints, &localInfo) == 0 && localInfo) {
                            for (auto* ptr = localInfo; ptr; ptr = ptr->ai_next) {
                                if (ptr->ai_family == AF_INET) {
                                    auto* sinp = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
                                    inet_ntop(AF_INET, &sinp->sin_addr, serverIPAddr, INET_ADDRSTRLEN);
                                    break;
                                }
                            }
                            freeaddrinfo(localInfo);
                        }
                    }
                    in_addr serverIP;
                    inet_pton(AF_INET, serverIPAddr, &serverIP);

                    // Build RSP_DOWNLOAD response.
                    std::vector<uint8_t> response(19 + filenameLen);
                    response[0] = RSP_DOWNLOAD;
                    *reinterpret_cast<uint32_t*>(&response[1]) = serverIP.s_addr;
                    *reinterpret_cast<uint16_t*>(&response[5]) = transferAddr.sin_port; // Already in network byte order
                    *reinterpret_cast<uint32_t*>(&response[7]) = htonl(sessionId);
                    *reinterpret_cast<uint32_t*>(&response[11]) = htonl(fileSize);
                    *reinterpret_cast<uint32_t*>(&response[15]) = htonl(filenameLen);
                    memcpy(&response[19], filename.data(), filenameLen);
                    send(client,
                        reinterpret_cast<const char*>(response.data()),
                        static_cast<int>(response.size()),
                        0);

                    // Setup client UDP address using provided IP and port.
                    sockaddr_in clientUdpAddr{};
                    clientUdpAddr.sin_family = AF_INET;
                    clientUdpAddr.sin_addr.s_addr = clientIP;
                    clientUdpAddr.sin_port = htons(clientPort);

                    {
                        std::lock_guard<std::mutex> lock(stdoutMutex);
                        char ipStr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &clientUdpAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
                        std::cout << "Starting file transfer - sessionId: " << sessionId
                            << ", file: " << filename
                            << ", size: " << fileSize << " bytes"
                            << ", to: " << ipStr << ":" << ntohs(clientUdpAddr.sin_port) << std::endl;
                    }

                    // Launch a background thread for the file transfer using our new UDP socket.
                    {
                        std::lock_guard<std::mutex> lock(transfersMutex);
                        activeTransfers[sessionId] = std::thread([=]() {
                            bool success = sendFileReliably(sessionId, fullPath, clientUdpAddr, transferSocket);
                            {
                                std::lock_guard<std::mutex> lk(stdoutMutex);
                                if (success) {
                                    std::cout << "Transfer complete - sessionId: " << sessionId << std::endl;
                                }
                                else {
                                    std::cout << "Transfer failed - sessionId: " << sessionId << std::endl;
                                }
                            }
                            closesocket(transferSocket); // Close our dedicated socket
                            {
                                std::lock_guard<std::mutex> lk(transfersMutex);
                                activeTransfers.erase(sessionId);
                            }
                            });
                        activeTransfers[sessionId].detach();
                    }
                    break;
                }
                default:
                    // Unknown command: simply remove it
                    buffer.erase(buffer.begin());
                    break;
                }
            }
            catch (...) {
                valid = false;
            }
            if (!valid) break;
        }
    }

disconnect:
    // Remove client from connected clients list
    {
        std::lock_guard<std::mutex> lock(stdoutMutex);
        auto it = connectedClients.find({ ipHost, port });
        if (it != connectedClients.end()) {
            connectedClients.erase(it);
            std::cout << "Client disconnected - IP: " << inet_ntoa(clientAddr.sin_addr)
                << " Port: " << port << std::endl;
        }
    }

    closesocket(client);
    return true;
}

/**
 * @brief Performs graceful shutdown of a socket
 * @param socket Socket to shutdown
 *
 * Ensures proper cleanup by:
 * 1. Shutting down both send and receive
 * 2. Closing the socket
 * 3. Setting the socket handle to INVALID_SOCKET
 */
void gracefulShutdown(SOCKET& socket) {
    if (socket != INVALID_SOCKET) {
        shutdown(socket, SD_BOTH);      // Shutdown both directions
        closesocket(socket);            // Close the socket
        socket = INVALID_SOCKET;        // Invalidate the handle
    }
}