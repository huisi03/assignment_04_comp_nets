/******************************************************************************/
/*!
\file		GameState_Asteroids.h
\author		
\par		
\date		
\brief		This file contains the declarations of functions to run the Asteroids
			Game State which includes Load, Init, Update, Draw, Free and Unload.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#ifndef CSD1130_GAME_STATE_PLAY_H_
#define CSD1130_GAME_STATE_PLAY_H_

#include "AEVec2.h"

// ---------------------------------------------------------------------------

void GameStateAsteroidsLoad(void);
void GameStateAsteroidsInit(void);
void GameStateAsteroidsUpdate(void);
void GameStateAsteroidsDraw(void);
void GameStateAsteroidsFree(void);
void GameStateAsteroidsUnload(void);

unsigned long LoadHighScore();
void SaveHighScore(unsigned long highscore);
void RenderText(AEVec2 position, f32 fontSize, char const* text);


extern long					sShipLives;
// ---------------------------------------------------------------------------

#endif // CSD1130_GAME_STATE_PLAY_H_


