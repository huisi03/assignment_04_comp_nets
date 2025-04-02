/******************************************************************************/
/*!
\file		Main.h
\author		
\par		
\date		
\brief		This file contains the headers to be included in the application.
			It also contains declarations for the delta time and application
			time for the driver file main.cpp.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#ifndef CSD1130_MAIN_H_
#define CSD1130_MAIN_H_ // header guards

//------------------------------------
// Globals

enum class GameType {
    SINGLE_PLAYER,
    MULTIPLAYER,
    SERVER
};

extern float	g_dt;
extern double	g_appTime;
extern int		pFont; // this is for the font which is used to display the text. 
extern GameType gameType;



// ---------------------------------------------------------------------------
// includes

#include "AEEngine.h"				// alpha engine
#include "Math.h"					// math library

#include "GameStateMgr.h"			// game state manager
#include "GameState_Asteroids.h"	// game state: asteroids
#include "Collision.h"				// collision
#include "GameState_Mainmenu.h"     //Main Menu
#include <iostream>

#endif // CSD1130_MAIN_H_
