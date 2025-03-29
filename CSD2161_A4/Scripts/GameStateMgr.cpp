/******************************************************************************/
/*!
\file		GameStateMgr.cpp
\author 	
\par    	
\date   	
\brief		This file contains definitions of functions to initialise the game states, 
			and update and set the pointers for the game states

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "GameStateMgr.h"			// main header
#include "GameState_Asteroids.h"	// game state
#include <AEEngine.h>

// ---------------------------------------------------------------------------
// globals

// variables to keep track the current, previous and next game state
unsigned int	gGameStateInit;
unsigned int	gGameStateCurr;
unsigned int	gGameStatePrev;
unsigned int	gGameStateNext;

// pointer to functions for game state life cycles functions
void (*GameStateLoad)()		= 0;
void (*GameStateInit)()		= 0;
void (*GameStateUpdate)()	= 0;
void (*GameStateDraw)()		= 0;
void (*GameStateFree)()		= 0;
void (*GameStateUnload)()	= 0;

/******************************************************************************/
/*!
\brief
	Initialise the game state manager and sets the initial starting state

\param[in] gameStateInit (unsigned int)
	- The starting state index of the game state manager

\return void
*/
/******************************************************************************/
void GameStateMgrInit(unsigned int gameStateInit)
{
	// set the initial game state
	gGameStateInit = gameStateInit;

	// reset the current, previoud and next game
	gGameStateCurr = 
	gGameStatePrev = 
	gGameStateNext = gGameStateInit;

	// call the update to set the function pointers
	GameStateMgrUpdate();
}

/******************************************************************************/
/*!
\brief
	Update and set the pointers to functions based on the current
	game state

\return void
*/
/******************************************************************************/
void GameStateMgrUpdate()
{
	// these states will be handled in the main
	if ((gGameStateCurr == GS_RESTART) || (gGameStateCurr == GS_QUIT))
		return;

	// switch case based on the current game state
	// sets the pointers to functions in the header files
	switch (gGameStateCurr)
	{
		// Assign Asteroid Game State Pointers
		case GS_ASTEROIDS:
			
			GameStateLoad = GameStateAsteroidsLoad;
			GameStateInit = GameStateAsteroidsInit;
			GameStateUpdate = GameStateAsteroidsUpdate;
			GameStateDraw = GameStateAsteroidsDraw;
			GameStateFree = GameStateAsteroidsFree;
			GameStateUnload = GameStateAsteroidsUnload;
			break;

		// exception
		default:
			AE_FATAL_ERROR("invalid state!!");
	}
}