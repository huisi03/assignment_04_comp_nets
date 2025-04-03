/******************************************************************************/
/*!
\file		GameState_MainMenu.cpp
\author 	Nivethavarsni, nivethavarsni.d, 2301537
\par    	email: nivethavarsni.d\@digipen.edu
\date   	March 29, 2025
\brief		This file contains the definitions for the function that are needed
			for the mainmenu. 

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#include "main.h"
#include <ctime>   // For getting the current date and time

// Function to get the current date and time as a string
std::string GetCurrentDateTime()
{
	// Get current time as time_t object
	std::time_t currentTime = std::time(nullptr);

	// Convert to tm struct for local timezone using localtime_s 
	std::tm localTime;
	localtime_s(&localTime, &currentTime);  // Use localtime_s instead of localtime

	// Format the time as "YYYY-MM-DD HH:MM:SS"
	char buffer[80];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);

	return std::string(buffer); // return formatted string
}


const AEVec2 Utilities::Worldtoscreencoordinates(AEVec2 const world)
{
	AEVec2 screen = { 0.f, 0.f }; // initialise the screen coordinates to 0,0
	screen.x = world.x / ((AEGfxGetWindowWidth() * 0.5f)); // convert the x world coordinate to screen.
	screen.y = world.y / ((AEGfxGetWindowHeight() * 0.5f)); // convert the y world coordinate to screen. 
	return screen; // return the screen coordinates. 
}

void GameStateMainMenuLoad(void) {
	// nothing is needed here. 
}

void GameStateMainMenuInit(void) {
	// nothing is needed here. 
}


void GameStateMainMenuUpdate(void) {

    if (gameType == GameType::SINGLE_PLAYER) {

        if (AEInputCheckTriggered(AEVK_P)) { // if P is triggered then it will go to level 1. 

            gGameStateNext = GS_ASTEROIDS;

        } else if (AEInputCheckTriggered(AEVK_Q)) {

            gGameStateNext = GS_QUIT; // if Q is triggereed then it quits the game. 
        }
    }

}


void GameStateMainMenuDraw(void) {
	AEGfxSetBackgroundColor(0.f, 0.f, 0.f); // all 0 for black. 
	char strBuffer[100]; // initialise a string buffer for storing the text. 
	AEVec2 textposition{}; // this is to set the position of the text 

	textposition.x = -130.f;
	textposition.y = 135.f;
	textposition = Utilities::Worldtoscreencoordinates(textposition);  
	memset(strBuffer, 0, 100 * sizeof(char));
	sprintf_s(strBuffer, "Asteroids Game");
	AEGfxPrint((s8)pFont, strBuffer, textposition.x, textposition.y, 1.f, 1.f, 0.f, 1.f, 1.f); // prints the game name. 

    textposition.x = -130.f;
    textposition.y = 75.f;
    textposition = Utilities::Worldtoscreencoordinates(textposition);
    memset(strBuffer, 0, 100 * sizeof(char));
    if (gameType == GameType::SINGLE_PLAYER) {
        sprintf_s(strBuffer, "Press 'P' to start playing"); // click on to play level
    }
    else if (gameType == GameType::MULTIPLAYER) {
        sprintf_s(strBuffer, "Press 'L' to enter Game lobby"); // click on L to enter game loby
    }
    AEGfxPrint((s8)pFont, strBuffer, textposition.x, textposition.y, 1.f, 1.f, 1.f, 1.f, 1.f);

	textposition.x = -130.f;
	textposition.y = 15.f;
	textposition = Utilities::Worldtoscreencoordinates(textposition);
	memset(strBuffer, 0, 100 * sizeof(char));
	sprintf_s(strBuffer, "Press 'Q' to Quit"); // click Q to exit from the game. 
	AEGfxPrint((s8)pFont, strBuffer, textposition.x, textposition.y, 1.f, 1.f, 1.f, 1.f, 1.f);


	// Display current date and time
	std::string currentDateTime = GetCurrentDateTime(); // Get current date and time as a string 
	textposition.x = -130.f; // Set x position for the date/time 
	textposition.y = 250.f; // Set y position for the date/time 
	textposition = Utilities::Worldtoscreencoordinates(textposition);  // Convert to screen coordinates 
	memset(strBuffer, 0, 100 * sizeof(char));
	sprintf_s(strBuffer, "Current Date & Time: %s", currentDateTime.c_str()); // Format the text 
	AEGfxPrint((s8)pFont, strBuffer, textposition.x, textposition.y, 1.f, 1.f, 1.f, 1.f, 1.f); // Draw the date/time on the screen 
}


void GameStateMainMenuFree(void) {
	// nothing is needed here
}


void GameStateMainMenuUnload(void) {
	// nothing is needed here. 
}

