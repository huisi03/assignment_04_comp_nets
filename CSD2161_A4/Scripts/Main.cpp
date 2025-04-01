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

#include <memory>		// for memory leaks

// ---------------------------------------------------------------------------
// Globals
float	    g_dt;
double	    g_appTime;
int			pFont; // this is for the text
const int	Fontsize = 25; // size of the text
GameType    gameType;

void BeginGameLoop(HINSTANCE instanceH, int show);
void ProcessMultiplayerClientInput();

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

	// Enable run-time memory check for debug builds.
	#if defined(DEBUG) | defined(_DEBUG)
		_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	#endif

	// Attach a console for input/output
	AttachConsoleWindow(); //hello

	// Initialize network
	if (InitialiseNetwork() != 0)
	{
		std::cerr << "Error Connecting to Network" << std::endl;
	}


	// Game loop
	if (networkType == NetworkType::SERVER)
	{
        gameType = GameType::SERVER;

		std::cout << "Processing Server..." << std::endl;

        int clientsRequired = 2;
        bool is_game_start = false;

		while (true) {

            sockaddr_in address{};
			NetworkPacket packet = ReceivePacket(udpServerSocket, address);

            if (packet.commandID == REQ_CONNECT) {

                HandleConnectionRequest(udpServerSocket, address, packet);

            } else if (packet.commandID == REQ_GAME_START) {

                if (clientsJoiningGame.size() >= clientsRequired)
                {
                    // ignore request, lobby is full
                    continue;
                }

                HandleJoinRequest(udpServerSocket, address, packet);

			} else if (packet.commandID == REQ_QUIT) {

                HandleQuitRequest(udpServerSocket, address, packet);

            } else if (clientsJoiningGame.size() >= clientsRequired && !is_game_start) {

				for (sockaddr_in const addr : clientsJoiningGame)
				{
					SendGameStateStart(udpServerSocket, addr);
				}
                is_game_start = true;
                                
			}

            // placeholder code
            if (is_game_start) {
                if (clientTargetAddresses.size() == 0) {
                    break;
                }
            }
                
		}

		Disconnect();
	}
	else if (networkType == NetworkType::CLIENT)
	{
        gameType = GameType::MULTIPLAYER;

		std::cout << "Processing Client..." << std::endl;

        BeginGameLoop(instanceH, show);
        

		Disconnect();
	}
	else if (networkType == NetworkType::SINGLE_PLAYER)
	{
        gameType = GameType::SINGLE_PLAYER;

        BeginGameLoop(instanceH, show);
	}

	// Wait for the user to press any key
	std::cout << "Press any key to exit..." << std::endl;
	std::cin.get();

	//free you font here
	AEGfxDestroyFont((s8)pFont);

	// free console
	FreeConsoleWindow();
}

void ProcessMultiplayerClientInput() {

    sockaddr_in address{};
    NetworkPacket packet = ReceivePacket(udpClientSocket, address);

    if (AEInputCheckTriggered(AEVK_L) && gGameStateCurr == GS_MAINMENU) {

        SendJoinRequest(udpClientSocket, serverTargetAddress);

        std::cout << "Joined the lobby successfully!" << std::endl;
        std::cout << "Waiting for game to start..." << std::endl;
        
    } else if (AEInputCheckTriggered(AEVK_Q)) {

        SendQuitRequest(udpClientSocket, serverTargetAddress);
        gGameStateNext = GS_QUIT;
    }

    if (HandleGameStateStart(packet)) {
        gGameStateNext = GS_ASTEROIDS;
    }

    // Resend lost packets if needed
    RetransmitPacket();
}

void BeginGameLoop(HINSTANCE instanceH, int show) {

    // Initialize the system
    AESysInit(instanceH, show, 800, 600, 1, 60, false, NULL);

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
        while (gGameStateCurr == gGameStateNext)
        {
            AESysFrameStart(); // start of frame

            if (gameType == GameType::MULTIPLAYER) {

                ProcessMultiplayerClientInput();
            }

            GameStateUpdate(); // update current game state

            GameStateDraw(); // draw current game state

            AESysFrameEnd(); // end of frame

            // check if forcing the application to quit
            if (AESysDoesWindowExist() == false)
            {
                gGameStateNext = GS_QUIT;
                SendQuitRequest(udpClientSocket, serverTargetAddress);
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