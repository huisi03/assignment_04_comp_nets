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

extern float	g_dt;
extern double	g_appTime;

// ---------------------------------------------------------------------------
// includes

#include "AEEngine.h"				// alpha engine
#include "Math.h"					// math library

#include "GameStateMgr.h"			// game state manager
#include "GameState_Asteroids.h"	// game state: asteroids
#include "Collision.h"				// collision

#endif