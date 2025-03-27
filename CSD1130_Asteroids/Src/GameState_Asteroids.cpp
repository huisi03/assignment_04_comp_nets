/******************************************************************************/
/*!
\file		GameState_Asteroids.cpp
\author 	
\par    	
\date   	
\brief		This file contains the implementation of the "asteroids" game state.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/


#include "main.h"
#include <stdlib.h>

/******************************************************************************/
/*!
	Defines
*/
/******************************************************************************/
#define RAND_FLOAT(min,max) ((AERandFloat()/1.0f)*((max)-(min)) + min) // generate a random number from a given range

const unsigned int  SCORE_CAP = 5000;
const unsigned int  SCORE_INCREMENT = 100;

const unsigned int	GAME_OBJ_NUM_MAX = 32;				// The total number of different objects (Shapes)
const unsigned int	GAME_OBJ_INST_NUM_MAX = 2048;		// The total number of different game object instances


const unsigned int	SHIP_INITIAL_NUM = 3;				// initial number of ship lives
const float			SHIP_SCALE_X = 16.0f;				// ship scale x
const float			SHIP_SCALE_Y = 16.0f;				// ship scale y
const float			BULLET_SCALE_X = 20.0f;				// bullet scale x
const float			BULLET_SCALE_Y = 3.0f;				// bullet scale y
const float			ASTEROID_MIN_SCALE_X = 10.0f;		// asteroid minimum scale x
const float			ASTEROID_MAX_SCALE_X = 60.0f;		// asteroid maximum scale x
const float			ASTEROID_MIN_SCALE_Y = 10.0f;		// asteroid minimum scale y
const float			ASTEROID_MAX_SCALE_Y = 60.0f;		// asteroid maximum scale y

const float			WALL_SCALE_X = 64.0f;				// wall scale x
const float			WALL_SCALE_Y = 164.0f;				// wall scale y

const float			SHIP_ACCEL_FORWARD = 100.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_ACCEL_BACKWARD = 100.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_ROT_SPEED = (2.0f * PI);		// ship rotation speed (degree/second)

const float			BULLET_SPEED = 400.0f;				// bullet speed (m/s)
	
const float         BOUNDING_RECT_SIZE = 1.0f;         // this is the normalized bounding rectangle (width and height) sizes - AABB collision data

// -----------------------------------------------------------------------------
enum TYPE
{
	// list of game object types
	TYPE_SHIP = 0,
	TYPE_BULLET,
	TYPE_ASTEROID,
	TYPE_WALL,

	TYPE_NUM
};

// -----------------------------------------------------------------------------
// object flag definition

const unsigned long FLAG_ACTIVE = 0x00000001;

/******************************************************************************/
/*!
	Struct/Class Definitions
*/
/******************************************************************************/

//Game object structure
struct GameObj
{
	unsigned long		type;		// object type
	AEGfxVertexList* pMesh;			// This will hold the triangles which will form the shape of the object
};

// ---------------------------------------------------------------------------

//Game object instance structure
struct GameObjInst
{
	GameObj* pObject;				// pointer to the 'original' shape
	unsigned long		flag;		// bit flag or-ed together
	AEVec2				scale;		// scaling value of the object instance
	AEVec2				posCurr;	// object current position

	AEVec2				posPrev;	// object previous position -> it's the position calculated in the previous loop

	AEVec2				velCurr;	// object current velocity
	float				dirCurr;	// object current direction
	AABB				boundingBox;// object bouding box that encapsulates the object
	AEMtx33				transform;	// object transformation matrix: Each frame, 
									// calculate the object instance's transformation matrix and save it here

};

/******************************************************************************/
/*!
	Static Variables
*/
/******************************************************************************/

// list of original object
static GameObj				sGameObjList[GAME_OBJ_NUM_MAX];				// Each element in this array represents a unique game object (shape)
static unsigned long		sGameObjNum;								// The number of defined game objects

// list of object instances
static GameObjInst			sGameObjInstList[GAME_OBJ_INST_NUM_MAX];	// Each element in this array represents a unique game object instance (sprite)
static unsigned long		sGameObjInstNum;							// The number of used game object instances

// pointer to the ship object
static GameObjInst* spShip;												// Pointer to the "Ship" game object instance

// pointer to the wall object
static GameObjInst* spWall;												// Pointer to the "Wall" game object instance

// number of ship available (lives 0 = game over)
static long					sShipLives;									// The number of lives left

// the score = number of asteroid destroyed
static unsigned long		sScore;										// Current score

static bool onValueChange = true;
// ---------------------------------------------------------------------------

// functions to create/destroy a game object instance
GameObjInst* gameObjInstCreate(unsigned long type, AEVec2* scale,
									AEVec2* pPos, AEVec2* pVel, float dir);
void				gameObjInstDestroy(GameObjInst* pInst);

void				Helper_Wall_Collision();


/******************************************************************************/
/*!
	"Load" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsLoad(void)
{
	// zero the game object array
	memset(sGameObjList, 0, sizeof(GameObj) * GAME_OBJ_NUM_MAX);
	// No game objects (shapes) at this point
	sGameObjNum = 0;

	// zero the game object instance array
	memset(sGameObjInstList, 0, sizeof(GameObjInst) * GAME_OBJ_INST_NUM_MAX);
	// No game object instances (sprites) at this point
	sGameObjInstNum = 0;

	// The ship object instance hasn't been created yet, so this "spShip" pointer is initialized to 0
	spShip = nullptr;

	// load/create the mesh data (game objects / Shapes)
	GameObj* pObj;

	// =====================
	// create the ship shape
	// =====================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_SHIP;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFFFF0000, 0.0f, 0.0f,
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 0.0f,
		0.5f, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =======================
	// create the bullet shape
	// =======================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BULLET;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");

	// =========================
	// create the asteroid shape
	// =========================
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_ASTEROID;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =========================
	// create the wall shape
	// =========================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_WALL;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0x6600FF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0x6600FF00, 0.0f, 0.0f,
		-0.5f, 0.5f, 0x6600FF00, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0x6600FF00, 0.0f, 0.0f,
		0.5f, -0.5f, 0x6600FF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0x6600FF00, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


}

/******************************************************************************/
/*!
	"Initialize" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsInit(void)
{
	// create the main ship
	AEVec2 scale;
	AEVec2Set(&scale, SHIP_SCALE_X, SHIP_SCALE_Y);
	spShip = gameObjInstCreate(TYPE_SHIP, &scale, nullptr, nullptr, 0.0f);
	AE_ASSERT(spShip);


	// create the initial 4 asteroids instances using the "gameObjInstCreate" function
	AEVec2 pos, vel;

	//Asteroid 1
	pos.x = 90.0f;		pos.y = -220.0f;
	vel.x = -60.0f;		vel.y = -30.0f;
	AEVec2Set(&scale, ASTEROID_MIN_SCALE_X, ASTEROID_MAX_SCALE_Y);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	//Asteroid 2
	pos.x = -260.0f;	pos.y = -250.0f;
	vel.x = 39.0f;		vel.y = -130.0f;
	AEVec2Set(&scale, ASTEROID_MAX_SCALE_X, ASTEROID_MIN_SCALE_Y);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	//Asteroid 3
	pos.x = -200.0f;	pos.y = -280.0f;
	vel.x = 40.0f;		vel.y = -10.0f;
	AEVec2Set(&scale, ASTEROID_MIN_SCALE_X, ASTEROID_MIN_SCALE_Y);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);


	//Asteroid 4
	pos.x = 500.0f;		pos.y = 50.0f;
	vel.x = 10.0f;		vel.y = 90.0f;
	AEVec2Set(&scale, ASTEROID_MAX_SCALE_X, ASTEROID_MAX_SCALE_Y);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	// create the static wall
	AEVec2Set(&scale, WALL_SCALE_X, WALL_SCALE_Y);
	AEVec2 position;
	AEVec2Set(&position, 300.0f, 150.0f);
	spWall = gameObjInstCreate(TYPE_WALL, &scale, &position, nullptr, 0.0f);
	AE_ASSERT(spWall);


	// reset the score and the number of ships
	sScore = 0;
	sShipLives = SHIP_INITIAL_NUM;
}

/******************************************************************************/
/*!
	"Update" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsUpdate(void)
{
	// =========================================================
	// update according to input
	// =========================================================

	// This input handling moves the ship without any velocity nor acceleration
	// It should be changed when implementing the Asteroids project
	//
	// Updating the velocity and position according to acceleration is 
	// done by using the following:
	// Pos1 = 1/2 * a*t*t + v0*t + Pos0
	//
	// In our case we need to divide the previous equation into two parts in order 
	// to have control over the velocity and that is done by:
	//
	// v1 = a*t + v0		//This is done when the UP or DOWN key is pressed 
	// Pos1 = v1*t + Pos0

	if (AEInputCheckCurr(AEVK_UP) && sShipLives >= 0 && sScore < SCORE_CAP)
	{
		AEVec2 velocity;
		AEVec2Set(&velocity, cosf(spShip->dirCurr), sinf(spShip->dirCurr)); // get normalized acceleration
		AEVec2Scale(&velocity, &velocity, SHIP_ACCEL_FORWARD); // get full acceleration vector

		// Find the velocity according to the acceleration
		AEVec2Scale(&velocity, &velocity, g_dt); // change in velocity
		// Update current ship velocity
		AEVec2Add(&spShip->velCurr, &velocity, &spShip->velCurr);
		// Limit your speed over here
		AEVec2Scale(&spShip->velCurr, &spShip->velCurr, 0.99f); // newVal * 0.99

	}

	if (AEInputCheckCurr(AEVK_DOWN) && sShipLives >= 0 && sScore < SCORE_CAP)
	{
		AEVec2 velocity;
		AEVec2Set(&velocity, cosf(spShip->dirCurr), sinf(spShip->dirCurr)); // get normalized acceleration
		AEVec2Scale(&velocity, &velocity, -SHIP_ACCEL_BACKWARD); // get full acceleration vector

		// Find the velocity according to the acceleration
		AEVec2Scale(&velocity, &velocity, g_dt); // change in velocity
		// Update current ship velocity
		AEVec2Add(&spShip->velCurr, &velocity, &spShip->velCurr);
		// Limit your speed over here
		AEVec2Scale(&spShip->velCurr, &spShip->velCurr, 0.99f); // newVal * 0.99

	}

	if (AEInputCheckCurr(AEVK_LEFT) && sShipLives >= 0 && sScore < SCORE_CAP)
	{
		spShip->dirCurr += SHIP_ROT_SPEED * (float)(AEFrameRateControllerGetFrameTime());
		spShip->dirCurr = AEWrap(spShip->dirCurr, -PI, PI);
	}

	if (AEInputCheckCurr(AEVK_RIGHT) && sShipLives >= 0 && sScore < SCORE_CAP)
	{
		spShip->dirCurr -= SHIP_ROT_SPEED * (float)(AEFrameRateControllerGetFrameTime());
		spShip->dirCurr = AEWrap(spShip->dirCurr, -PI, PI);
	}


	// Shoot a bullet if space is triggered (Create a new object instance)
	if (AEInputCheckTriggered(AEVK_SPACE) && sShipLives >= 0 && sScore < SCORE_CAP)
	{
		// Get the bullet's direction according to the ship's direction
		AEVec2 velocity;
		AEVec2Set(&velocity, cosf(spShip->dirCurr), sinf(spShip->dirCurr));
		// Set the velocity
		AEVec2Scale(&velocity, &velocity, BULLET_SPEED);
		// Create an instance, based on BULLET_SCALE_X and BULLET_SCALE_Y
		AEVec2 pos, scale;
		AEVec2Set(&pos, spShip->posCurr.x, spShip->posCurr.y);
		AEVec2Set(&scale, BULLET_SCALE_X, BULLET_SCALE_Y);
 		gameObjInstCreate(TYPE_BULLET, &scale, &pos, &velocity, spShip->dirCurr);

	}






	// ======================================================================
	// Save previous positions
	//  -- For all instances
	// [DO NOT UPDATE THIS PARAGRAPH'S CODE]
	// ======================================================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		pInst->posPrev.x = pInst->posCurr.x;
		pInst->posPrev.y = pInst->posCurr.y;
	}






	// ======================================================================
	// update physics of all active game object instances
	//  -- Calculate the AABB bounding rectangle of the active instance, using the starting position:
	//		boundingRect_min = -(BOUNDING_RECT_SIZE/2.0f) * instance->scale + instance->posPrev
	//		boundingRect_max = +(BOUNDING_RECT_SIZE/2.0f) * instance->scale + instance->posPrev
	//
	//	-- New position of the active instance is updated here with the velocity calculated earlier
	// ======================================================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		// Calculate AABB bounding rectangle of the active instance
		AEVec2 boundingBoxMin;
		AEVec2Scale(&boundingBoxMin, &pInst->scale, -(BOUNDING_RECT_SIZE / 2.0f));
		AEVec2Add(&boundingBoxMin, &boundingBoxMin, &pInst->posPrev);
		pInst->boundingBox.min = boundingBoxMin;

		AEVec2 boundingBoxMax;
		AEVec2Scale(&boundingBoxMax, &pInst->scale, (BOUNDING_RECT_SIZE / 2.0f));
		AEVec2Add(&boundingBoxMax, &boundingBoxMax, &pInst->posPrev);
		pInst->boundingBox.max = boundingBoxMax;

		// Update new position of the active instance
		AEVec2 newVel;
		AEVec2Scale(&newVel, &pInst->velCurr, g_dt);
		AEVec2Add(&pInst->posCurr, &newVel, &pInst->posCurr);

	}




	// ======================================================================
	// check for dynamic-static collisions (one case only: Ship vs Wall)
	// [DO NOT UPDATE THIS PARAGRAPH'S CODE]
	// ======================================================================
	Helper_Wall_Collision();





	// ======================================================================
	// check for dynamic-dynamic collisions
	// ======================================================================

	/*
	for each object instance: oi1
		if oi1 is not active
			skip

		if oi1 is an asteroid
			for each object instance oi2
				if(oi2 is not active or oi2 is an asteroid)
					skip

				if(oi2 is the ship)
					Check for collision between ship and asteroids (Rectangle - Rectangle)
					Update game behavior accordingly
					Update "Object instances array"
				else
				if(oi2 is a bullet)
					Check for collision between bullet and asteroids (Rectangle - Rectangle)
					Update game behavior accordingly
					Update "Object instances array"
	*/

	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		if (pInst->pObject->type == TYPE_ASTEROID) {
			for (unsigned long j = 0; j < GAME_OBJ_INST_NUM_MAX; j++) {
				
				GameObjInst* pInst2 = sGameObjInstList + j;
				
				if ((pInst2->flag & FLAG_ACTIVE) == 0 || pInst2->pObject->type == TYPE_ASTEROID) {
					continue;
				}

				float firstTimeOfCollision = 0.0f;
				if (pInst2->pObject->type == TYPE_SHIP) {
					
					if (CollisionIntersection_RectRect( pInst->boundingBox, pInst->velCurr,
						pInst2->boundingBox, pInst2->velCurr,firstTimeOfCollision)) {

						// if ship and asteroid collided
 						sShipLives -= 1;
						if (sShipLives >= -1) {
							printf("\nship collided with asteroid\n");
							onValueChange = true;
						}

						// destroy asteroid
						gameObjInstDestroy(pInst);

						// reset the ship
						AEVec2Zero(&spShip->velCurr);
						AEVec2Zero(&spShip->posCurr);

						// spawn asteroid outside of viewport
						AEVec2 pos, vel, scale;

						scale.x = RAND_FLOAT(ASTEROID_MIN_SCALE_X, ASTEROID_MAX_SCALE_X);
						scale.y = RAND_FLOAT(ASTEROID_MIN_SCALE_Y, ASTEROID_MAX_SCALE_Y);

						pos.x = RAND_FLOAT(AEGfxGetWinMaxX(), ASTEROID_MAX_SCALE_X+AEGfxGetWinMaxX());
						pos.y = RAND_FLOAT(AEGfxGetWinMinY() - ASTEROID_MAX_SCALE_Y, AEGfxGetWinMinY());

						vel.x = RAND_FLOAT(-100.f, 100.f);
						vel.y = RAND_FLOAT(-100.f, 100.f);

						float dir = RAND_FLOAT(0.f, 360.f);
						
						gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, dir);

					}
				} else if (pInst2->pObject->type == TYPE_BULLET) {
					if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr,
						pInst2->boundingBox, pInst2->velCurr, firstTimeOfCollision)) {

						// if bullet and asteroid collided
						if (sShipLives >= -1 && sScore < SCORE_CAP) {
							onValueChange = true;
							sScore += SCORE_INCREMENT;
						}

						// destroy asteroid
						gameObjInstDestroy(pInst);
						// destroy bullet
						gameObjInstDestroy(pInst2);

						// spawn 1 or 2 asteroid outside of viewport
						int numOfAsteriods = rand() % 2 + 1;
						for (int k = 0; k < numOfAsteriods; ++k) {

							AEVec2 pos, vel, scale;
 							pos.x = RAND_FLOAT(AEGfxGetWinMinX() - ASTEROID_MAX_SCALE_X, AEGfxGetWinMinX());
							pos.y = RAND_FLOAT(AEGfxGetWinMaxY(), AEGfxGetWinMaxY() + ASTEROID_MAX_SCALE_Y);

							vel.x = RAND_FLOAT(-100.f, 100.f);
							vel.y = RAND_FLOAT(-100.f, 100.f);

							scale.x = RAND_FLOAT(ASTEROID_MIN_SCALE_X, ASTEROID_MAX_SCALE_X);
							scale.y = RAND_FLOAT(ASTEROID_MIN_SCALE_Y, ASTEROID_MAX_SCALE_Y);

							float dir = RAND_FLOAT(0.f, 360.f);
		
							gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, dir);
						}

					}
				}

			}
		}
	}





	// ===================================================================
	// update active game object instances
	// Example:
	//		-- Wrap specific object instances around the world (Needed for the assignment)
	//		-- Removing the bullets as they go out of bounds (Needed for the assignment)
	//		-- If you have a homing missile for example, compute its new orientation 
	//			(Homing missiles are not required for the Asteroids project)
	//		-- Update a particle effect (Not required for the Asteroids project)
	// ===================================================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		// check if the object is a ship
		if (pInst->pObject->type == TYPE_SHIP)
		{
			// Wrap the ship from one end of the screen to the other
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - SHIP_SCALE_X,
				AEGfxGetWinMaxX() + SHIP_SCALE_X);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - SHIP_SCALE_Y,
				AEGfxGetWinMaxY() + SHIP_SCALE_Y);
		}

		// Wrap asteroids here
		if (pInst->pObject->type == TYPE_ASTEROID)
		{
			// Wrap the asteroid from one end of the screen to the other
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - ASTEROID_MAX_SCALE_X,
				AEGfxGetWinMaxX() + ASTEROID_MAX_SCALE_X);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - ASTEROID_MAX_SCALE_Y,
				AEGfxGetWinMaxY() + ASTEROID_MAX_SCALE_Y);
		}

		// Remove bullets that go out of bounds
		if (pInst->pObject->type == TYPE_BULLET) {
			if (pInst->posCurr.x < AEGfxGetWinMinX() || pInst->posCurr.x > AEGfxGetWinMaxX()
				|| pInst->posCurr.y < AEGfxGetWinMinY() || pInst->posCurr.y > AEGfxGetWinMaxY()) {
				// bullet went out of bounds
				gameObjInstDestroy(pInst);
			}
		}
	}





	// =====================================================================
	// calculate the matrix for all objects
	// =====================================================================

	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;
		AEMtx33		 trans, rot, scale;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		// Compute the scaling matrix
		scale = { 0 };
		AEMtx33Scale(&scale, pInst->scale.x, pInst->scale.y);
		// Compute the rotation matrix 
		rot = { 0 };
		AEMtx33Rot(&rot, pInst->dirCurr);
		// Compute the translation matrix
		trans = { 0 };
		AEMtx33Trans(&trans, pInst->posCurr.x, pInst->posCurr.y);
		// Concatenate the 3 matrix in the correct order in the object instance's "transform" matrix
		AEMtx33Concat(&pInst->transform, &rot, &scale);
		AEMtx33Concat(&pInst->transform, &trans, &pInst->transform);

		UNREFERENCED_PARAMETER(trans);
		UNREFERENCED_PARAMETER(rot);
		UNREFERENCED_PARAMETER(scale);

	}
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void GameStateAsteroidsDraw(void)
{
	char strBuffer[1024];

	AEGfxSetRenderMode(AE_GFX_RM_COLOR);
	AEGfxTextureSet(NULL, 0, 0);


	// Set blend mode to AE_GFX_BM_BLEND
	// This will allow transparency.
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxSetTransparency(1.0f);


	// draw all object instances in the list
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		// Set the current object instance's transform matrix using "AEGfxSetTransform"
		// Draw the shape used by the current object instance using "AEGfxMeshDraw"
		AEGfxSetTransform(pInst->transform.m);
		AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);

	}

	//You can replace this condition/variable by your own data.
	//The idea is to display any of these variables/strings whenever a change in their value happens
	if (onValueChange)
	{
		sprintf_s(strBuffer, "Score: %d", sScore);
		//AEGfxPrint(10, 10, (u32)-1, strBuffer);
		printf("%s \n", strBuffer);

		sprintf_s(strBuffer, "Ship Left: %d", sShipLives >= 0 ? sShipLives : 0);
		//AEGfxPrint(600, 10, (u32)-1, strBuffer);
		printf("%s \n", strBuffer);

		// display the game over message
		if (sShipLives < 0)
		{
			//AEGfxPrint(280, 260, 0xFFFFFFFF, "       GAME OVER       ");
			printf("       GAME OVER       \n");
		}
		if (sScore >= SCORE_CAP) {
			printf("YOU ROCK!\n");
		}

		onValueChange = false;
	}
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void GameStateAsteroidsFree(void)
{
	// kill all object instances in the array using "gameObjInstDestroy"
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;
		
		gameObjInstDestroy(pInst);

	}

}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void GameStateAsteroidsUnload(void)
{
	// free all mesh data (shapes) of each object using "AEGfxTriFree"
	for (unsigned long i = 0; i < GAME_OBJ_NUM_MAX; i++)
	{
		GameObj* pObj = sGameObjList + i;

		if (pObj->pMesh) {
			AEGfxMeshFree(pObj->pMesh);
		}
	}
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
GameObjInst* gameObjInstCreate(unsigned long type,
	AEVec2* scale,
	AEVec2* pPos,
	AEVec2* pVel,
	float dir)
{
	AEVec2 zero;
	AEVec2Zero(&zero);

	AE_ASSERT_PARM(type < sGameObjNum);

	// loop through the object instance list to find a non-used object instance
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// check if current instance is not used
		if (pInst->flag == 0)
		{
			// it is not used => use it to create the new instance
			pInst->pObject = sGameObjList + type;
			pInst->flag = FLAG_ACTIVE;
			pInst->scale = *scale;
			pInst->posCurr = pPos ? *pPos : zero;
			pInst->velCurr = pVel ? *pVel : zero;
			pInst->dirCurr = dir;

			// return the newly created instance
			return pInst;
		}
	}

	// cannot find empty slot => return 0
	return 0;
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void gameObjInstDestroy(GameObjInst* pInst)
{
	// if instance is destroyed before, just return
	if (pInst->flag == 0)
		return;

	// zero out the flag
	pInst->flag = 0;
}



/******************************************************************************/
/*!
	check for collision between Ship and Wall and apply physics response on the Ship
		-- Apply collision response only on the "Ship" as we consider the "Wall" object is always stationary
		-- We'll check collision only when the ship is moving towards the wall!
	[DO NOT UPDATE THIS PARAGRAPH'S CODE]
*/
/******************************************************************************/
void Helper_Wall_Collision()
{
	//calculate the vectors between the previous position of the ship and the boundary of wall
	AEVec2 vec1;
	vec1.x = spShip->posPrev.x - spWall->boundingBox.min.x;
	vec1.y = spShip->posPrev.y - spWall->boundingBox.min.y;
	AEVec2 vec2;
	vec2.x = 0.0f;
	vec2.y = -1.0f;
	AEVec2 vec3;
	vec3.x = spShip->posPrev.x - spWall->boundingBox.max.x;
	vec3.y = spShip->posPrev.y - spWall->boundingBox.max.y;
	AEVec2 vec4;
	vec4.x = 1.0f;
	vec4.y = 0.0f;
	AEVec2 vec5;
	vec5.x = spShip->posPrev.x - spWall->boundingBox.max.x;
	vec5.y = spShip->posPrev.y - spWall->boundingBox.max.y;
	AEVec2 vec6;
	vec6.x = 0.0f;
	vec6.y = 1.0f;
	AEVec2 vec7;
	vec7.x = spShip->posPrev.x - spWall->boundingBox.min.x;
	vec7.y = spShip->posPrev.y - spWall->boundingBox.min.y;
	AEVec2 vec8;
	vec8.x = -1.0f;
	vec8.y = 0.0f;
	if (
		(AEVec2DotProduct(&vec1, &vec2) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec2) <= 0.0f) ||
		(AEVec2DotProduct(&vec3, &vec4) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec4) <= 0.0f) ||
		(AEVec2DotProduct(&vec5, &vec6) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec6) <= 0.0f) ||
		(AEVec2DotProduct(&vec7, &vec8) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec8) <= 0.0f)
		)
	{
		float firstTimeOfCollision = 0.0f;
		if (CollisionIntersection_RectRect(spShip->boundingBox,
			spShip->velCurr,
			spWall->boundingBox,
			spWall->velCurr,
			firstTimeOfCollision))
		{
			//re-calculating the new position based on the collision's intersection time
			spShip->posCurr.x = spShip->velCurr.x * (float)firstTimeOfCollision + spShip->posPrev.x;
			spShip->posCurr.y = spShip->velCurr.y * (float)firstTimeOfCollision + spShip->posPrev.y;

			//reset ship velocity
			spShip->velCurr.x = 0.0f;
			spShip->velCurr.y = 0.0f;
		}
	}
}