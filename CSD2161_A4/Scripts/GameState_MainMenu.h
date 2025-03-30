/******************************************************************************/
/*!
\file		GameState_MainMenu.h
\author 	Nivethavarsni, nivethavarsni.d, 2301537
\par    	email: nivethavarsni.d\@digipen.edu
\date   	March 29, 2025
\brief		This is the soruce files which has the functions needed to call the
            main menu.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#ifndef CSD1130_GAME_STATE_MAINMENU_H_
#define CSD1130_GAME_STATE_MAINMENU_H_


// ---------------------------------------------------------------------------

/******************************************************************************/
/*!
There is nothing needed in this function. 
*/
/******************************************************************************/
void GameStateMainMenuLoad(void);
/******************************************************************************/
/*!
There is nothing needed in this function. 
*/
/******************************************************************************/
void GameStateMainMenuInit(void);
/******************************************************************************/
/*!
This function checks what keys are triggered and loads the level accordingly or
exits.
*/
/******************************************************************************/
void GameStateMainMenuUpdate(void);
/******************************************************************************/
/*!
This function draws the necessary wordings and sets the backgroud.
*/
/******************************************************************************/
void GameStateMainMenuDraw(void);
/******************************************************************************/
/*!
There is nothing needed in this function. 
*/
/******************************************************************************/
void GameStateMainMenuFree(void);
/******************************************************************************/
/*!
There is nothing needed in this function. 
*/
/******************************************************************************/
void GameStateMainMenuUnload(void);

// ---------------------------------------------------------------------------


namespace Utilities {
	const AEVec2 Worldtoscreencoordinates(AEVec2 const world);
}
#endif // CSD1130_GAME_STATE_PLAY_H_
