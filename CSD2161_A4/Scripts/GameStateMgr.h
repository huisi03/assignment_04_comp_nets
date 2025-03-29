/******************************************************************************/
/*!
\file		GameStateMgr.h
\author 	
\par    	
\date   	
\brief		This file contains declarations of functions to initialise the game states,
			and update and set the pointers for the game states

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#ifndef CSD1130_GAME_STATE_MGR_H_
#define CSD1130_GAME_STATE_MGR_H_

// ---------------------------------------------------------------------------
// include the list of game states

#include "GameStateList.h"

// ---------------------------------------------------------------------------
// externs

extern unsigned int gGameStateInit;
extern unsigned int gGameStateCurr;
extern unsigned int gGameStatePrev;
extern unsigned int gGameStateNext;

// ---------------------------------------------------------------------------

extern void (*GameStateLoad)();
extern void (*GameStateInit)();
extern void (*GameStateUpdate)();
extern void (*GameStateDraw)();
extern void (*GameStateFree)();
extern void (*GameStateUnload)();

// ---------------------------------------------------------------------------
// Function prototypes

/******************************************************************************/
/*!
\brief
	Initialise the game state manager and sets the initial starting state

\param[in] gameStateInit (unsigned int)
	- The starting state index of the game state manager

\return void
*/
/******************************************************************************/
// call this at the beginning and AFTER all game states are added to the manager
void GameStateMgrInit(unsigned int gameStateInit);

/******************************************************************************/
/*!
\brief
	Update and set the pointers to functions based on the current
	game state

\return void
*/
/******************************************************************************/
// update is used to set the function pointers
void GameStateMgrUpdate();

// ---------------------------------------------------------------------------

#endif