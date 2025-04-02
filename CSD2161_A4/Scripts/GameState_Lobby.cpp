/******************************************************************************/
/*!
\file		GameState_Lobby.cpp
\author
\par
\date
\brief		
Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#include "GameState_Lobby.h"
#include "GameStateMgr.h"
#include "Network.h"
#include "main.h"

#include <cstdio>
#include <string>

static double lobbyTimer = 10.0;
static bool countdownStarted = false;
int connectedClients = 0; 
static const int requiredClients = 1;
extern int pFont; 

void GameStateLobbyLoad() {
    // Nothing needed here
}

void GameStateLobbyInit() {
    countdownStarted;
    lobbyTimer; // Reset timer
    connectedClients; // Reset connected clients
}

void GameStateLobbyUpdate() {
    // Only update connectedClients from clientsJoiningGame if we're the server
    if (networkType == NetworkType::SERVER) {
        //connectedClients = static_cast<int>(clientsJoiningGame.size());
    }

    // Check if game state start packet received
    sockaddr_in addr{};
    NetworkPacket packet = ReceivePacket(udpClientSocket, addr);
    /*if (HandleGameStateStart(packet)) {
        gGameStateNext = GS_ASTEROIDS;
    }*/

    if (connectedClients >= 1 && !countdownStarted) {
        countdownStarted = true;
        lobbyTimer = 10.0;
    }

    if (countdownStarted) {
        lobbyTimer -= g_dt;
        if (lobbyTimer <= 0.0) {
            countdownStarted = false;
        }
    }

    //RetransmitPacket();
}


void GameStateLobbyDraw() {
    AEGfxSetRenderMode(AE_GFX_RM_COLOR);
    AEGfxSetBackgroundColor(0.f, 0.f, 0.f); // Black background

    char strBuffer[100];
    AEVec2 pos;

    // Centered "Game Lobby"
    sprintf_s(strBuffer, "Game Lobby");
    pos = { 0.0f, 100.0f };
    pos = Utilities::Worldtoscreencoordinates(pos);

    float width, height;
    AEGfxGetPrintSize((s8)pFont, strBuffer, 1.5f, &width, &height);
    AEGfxPrint((s8)pFont, strBuffer, pos.x - width / 2.f, pos.y - height / 2.f, 1.5f, 1, 1, 1, 1);

    // Centered "Connected Players"
    sprintf_s(strBuffer, "Connected Players: %d", connectedClients);
    pos = { 0.0f, 50.0f };
    pos = Utilities::Worldtoscreencoordinates(pos);

    AEGfxGetPrintSize((s8)pFont, strBuffer, 1.0f, &width, &height);
    AEGfxPrint((s8)pFont, strBuffer, pos.x - width / 2.f, pos.y - height / 2.f, 1.0f, 1, 1, 1, 1);

    if (countdownStarted) {
        sprintf_s(strBuffer, "Starting in %.1f seconds...", lobbyTimer);
    }
    else {
        sprintf_s(strBuffer, "Waiting for players...");
    }
    pos = { 0.0f, 20.0f };
    pos = Utilities::Worldtoscreencoordinates(pos);

    AEGfxGetPrintSize((s8)pFont, strBuffer, 1.0f, &width, &height);
    AEGfxPrint((s8)pFont, strBuffer, pos.x - width / 2.f, pos.y - height / 2.f, 1.0f, 1, 1, 1, 1);
}


void GameStateLobbyFree() {
    // nothing
}

void GameStateLobbyUnload() {
    // nothing
}
