/******************************************************************************/
/*!
\file		Main.cpp
\author		
\par		
\date		
\brief		This file contains the 'WinMain' function and is the driver to run the application. It
			initialises the systems and game state manager to run the game loop which includes Input
			Handling, Update and Rendering

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "Network.h"	// networking for multiplayer

#include "Main.h"		// main headers
#include <thread>
#include <atomic>

#include <memory>		// for memory leaks

// ---------------------------------------------------------------------------
// Globals
float	 g_dt;
double	 g_appTime;
int			pFont; // this is for the text
int	Fontsize = 25; // size of the text
GameType    gameType;
int         g_show;
HINSTANCE   g_instanceH;

/******************************************************************************/
/*!
\brief 
    Main driver function, start of the application. Program execution begins and ends here.

\return int
*/
/******************************************************************************/
int WINAPI WinMain(HINSTANCE instanceH, HINSTANCE prevInstanceH, LPSTR command_line, int show)
{
	// ignore these parameters
	UNREFERENCED_PARAMETER(prevInstanceH);
	UNREFERENCED_PARAMETER(command_line);

    g_show = show;
    g_instanceH = instanceH;

	// Enable run-time memory check for debug builds.
	#if defined(DEBUG) | defined(_DEBUG)
		_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	#endif

	// Attach a console for input/output
	AttachConsoleWindow();

	// Initialize network
	if (InitialiseNetwork() != 0)
	{
		std::cerr << "Error Connecting to Network" << std::endl;
	}


	// Game loop
	if (networkType == NetworkType::SERVER)
	{
		std::cout << "Processing Server..." << std::endl;

        gameType = GameType::SERVER;
		
		sockaddr_in address{};
		std::map<uint16_t, sockaddr_in> clients;                    // source_port : sockaddr_in mapping
        std::unordered_map<uint16_t, std::thread> clientThreads;

		int clientsRequired = 2;
		int clientCount = 0;

		bool gameStarted = false; // Add a flag

        while (!gameStarted)
        {
            NetworkPacket packet = ReceivePacket(udpServerSocket, address);

            if (packet.packetID == REQ_CONNECT) {

                // Sends acknowledgment
                HandleConnectionRequest(udpServerSocket, address, packet);

            }
            else if (packet.packetID == REQ_QUIT) {

                std::cout << "Disconnecting client at port number: " << std::to_string(packet.sourcePortNumber) << std::endl;
                clientThreads.erase(packet.sourcePortNumber);
                --clientCount;


            }
            else if (packet.packetID == JOIN_REQUEST) {

                if (clientCount == clientsRequired && clients.count(packet.sourcePortNumber) == false)
                {
                    // ignore request, lobby is full
                    continue;
                }

                HandleJoinRequest(udpServerSocket, address, packet);

                // Check if the client is in the map already
                if (clients.count(packet.sourcePortNumber) == false)
                {
                    // Add the client port to IP to map
                    clients[packet.sourcePortNumber] = address;
                    ++clientCount;

                    // Create the player data and store it with the port number as key
                    switch (clientCount)
                    {
                    case 1:
                    {
                        AEVec2 posVec{ 100, 100 };
                        AEVec2 scaleVec{ 16, 16 };

                        playerDataMap.emplace(packet.sourcePortNumber, PlayerData(posVec, scaleVec));
                        break;
                    }
                    case 2:
                    {
                        AEVec2 posVec{ 200, 200 };
                        AEVec2 scaleVec{ 16, 16 };

                        playerDataMap.emplace(packet.sourcePortNumber, PlayerData(posVec, scaleVec));
                        break;
                    }
                    case 3:
                    {
                        AEVec2 posVec{ 300, 300 };
                        AEVec2 scaleVec{ 16, 16 };

                        playerDataMap.emplace(packet.sourcePortNumber, PlayerData(posVec, scaleVec));
                        break;
                    }
                    case 4:
                    {
                        AEVec2 posVec{ 400, 400 };
                        AEVec2 scaleVec{ 16, 16 };

                        playerDataMap.emplace(packet.sourcePortNumber, PlayerData(posVec, scaleVec));
                        break;
                    }
                    }
                    std::cout << "Num Players: " << playerDataMap.size() << std::endl;
                }

                // Can start game when players is max
                if (clientCount == clientsRequired)
                {
                    // Stop accepting new clients
                    gameStarted = true;

                    for (auto& [p, addr] : clients)
                    {
                        if (playerDataMap.count(p)) // Check if key 'p' exists in the map
                        {
                            SendGameStateStart(udpServerSocket, addr, playerDataMap[p]);
                            // Start a thread for each client
                            clientThreads[p] = std::thread(HandleClientInput, udpServerSocket, p, std::ref(playerDataMap));
                        }
                        else
                        {
                            std::cerr << "Player " << p << " not found in playersData!" << std::endl;
                        }
                    }
                    // Setting the game state for all objects
                    gameDataState.playerCount = gameDataState.objectCount = clientCount;

                    // Set the player as the object in the game state
                    for (auto& [port, data] : playerDataMap)
                    {
                        static int i = 0;
                        gameDataState.objects[i].transform.position = data.transform.position;
                        gameDataState.objects[i].type = ObjectType::OBJ_SHIP;
                        gameDataState.objects[i].identifier = port;
                        ++i;
                    }

                }
            } else {
                if (packet.packetID != UINT16_MAX) {
                    std::cout << "Received unknown packet from " << packet.sourcePortNumber << std::endl;
                }
                
            }
        }
        
		// Start a thread to broadcast the game state
		std::thread gameStateThread(BroadcastGameState, udpServerSocket, std::ref(clients));
		// Run in the background
		gameStateThread.detach(); 

		// Start a thread to calculate the Game Loop
		std::thread gameLoopState(GameLoop, std::ref(clients));
		// Run in the background
		gameLoopState.detach();

		// After game starts, wait for all threads to finish
		for (auto& [p, thread] : clientThreads)
		{
			if (thread.joinable())
				thread.join();
		}


		// Collate scores onto the leaderboard
		LoadLeaderboard();

		// Get current time
		char timeBuffer[20];
		std::time_t currentTime = std::time(nullptr);
		std::tm localTime;
		localtime_s(&localTime, &currentTime);
		std::strftime(timeBuffer, 20, "%Y-%m-%d %H:%M:%S", &localTime);

		for (auto [id, score, lives] : gameDataState.playerData)
		{
			AddScoreToLeaderboard(id, "", score, timeBuffer);
		}
		SaveLeaderboard();

		// End of game, send the final leaderboard to the clients
		BroadcastLeaderboard(udpServerSocket, std::ref(clients));

		// Show leaderboard

		Disconnect(udpServerSocket);
	}
	else if (networkType == NetworkType::CLIENT)
	{
		std::cout << "Processing Client..." << std::endl;

        gameType = GameType::MULTIPLAYER;

        // Start the background thread for listening to game state updates
        std::thread receiveThread(ListenForUpdates, udpClientSocket, serverTargetAddress, std::ref(clientData));
        receiveThread.detach(); // Detach it to run in background

        // Start the background thread for rendering on the client side
        std::thread renderThread(Render, std::ref(gameDataState));
        renderThread.detach(); // Detach it to run in background

        //HWND clientWindow = GetConsoleWindow();

        while (true)
        {
            // Huisi: commenting this out because when the window rendering the graphics is in focus, input wont be handled.
            // For multiple client on the same computer
            //if (GetForegroundWindow() != clientWindow)
            //{
            //    Sleep(10); // Small delay to avoid 100% CPU usage
            //    continue;
            //}

            // Player has to send join request in order to have the HandleClientInput actually handle the input packets
            // So, while player is in the main menu, let the following code handle the player input instead
            if (gGameStateCurr == GS_MAINMENU) {
                if (GetAsyncKeyState('L') & 0x8000 ) {
                    SendJoinRequest(udpClientSocket, serverTargetAddress);
                }
                else if (GetAsyncKeyState('Q') & 0x8000) {
                    SendQuitRequest(udpClientSocket, serverTargetAddress);
                    gGameStateNext = GS_QUIT;
                    break;
                }
            } else {

                // Handle input
                NetworkPacket packet;
                packet.packetID = InputKey::NONE;

                if (GetAsyncKeyState(VK_UP) & 0x8000)      packet.packetID = InputKey::UP;
                else if (GetAsyncKeyState(VK_DOWN) & 0x8000) packet.packetID = InputKey::DOWN;
                else if (GetAsyncKeyState(VK_LEFT) & 0x8000) packet.packetID = InputKey::LEFT;
                else if (GetAsyncKeyState(VK_RIGHT) & 0x8000) packet.packetID = InputKey::RIGHT;
                else if (GetAsyncKeyState(VK_SPACE) & 0x8000) packet.packetID = InputKey::SPACE;
                else if (GetAsyncKeyState('Q') & 0x8000) {
                    SendQuitRequest(udpClientSocket, serverTargetAddress);
                    gGameStateNext = GS_QUIT;
                    break;
                }

                if (packet.packetID != InputKey::NONE) {
                    packet.sourcePortNumber = GetClientPort();
                    packet.destinationPortNumber = serverTargetAddress.sin_port;
                    SendPacket(udpClientSocket, serverTargetAddress, packet);
                }
            }

            RetransmitPacket();
            
        }

        if (gGameStateCurr == GS_END_GAME) {
            // End of game, receive leaderboard
            ReceiveLeaderboard(udpClientSocket);

            // Show leaderboard
        }


        Disconnect(udpClientSocket);

	}
	else if (networkType == NetworkType::SINGLE_PLAYER)
	{
        gameType = GameType::SINGLE_PLAYER;

        // Initialize the system
        AESysInit(instanceH, show, 800, 600, 1, 60, false, NULL);

        pFont = (int)AEGfxCreateFont("Resources/Arial Italic.ttf", Fontsize);

        // Changing the window title
        AESysSetWindowTitle("Spaceships!");

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
            while (gGameStateCurr == gGameStateNext)
            {
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

	// Wait for the user to press any key
	std::cout << "Press any key to exit..." << std::endl;
	std::cin.get();

	//free you font here
	AEGfxDestroyFont((s8)pFont);

	// free console
	FreeConsoleWindow();
}

