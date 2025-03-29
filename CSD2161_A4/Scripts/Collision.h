/******************************************************************************/
/*!
\file		Collision.h
\author		
\par		
\date		
\brief		This file contains the declarations of functions to check for collisions
			between objects

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#ifndef CSD1130_COLLISION_H_
#define CSD1130_COLLISION_H_ // header guard

#include "AEEngine.h" // alpha engine

/**************************************************************************/
/*!
\struct AABB
\brief
	Axis-aligned bounding box, consisting of 2 AEVec2 (2D Vector components).
	min representing the bottom left of the bounding box
	max representing the top right of the bounding box
	These data will be used to check for collision between objects
*/
/**************************************************************************/
struct AABB
{
	AEVec2	min;
	AEVec2	max;
};

/**************************************************************************/
/*!
\brief
	Checks the collision between 2 moving rects

\param[in] aabb1 (const AABB &)
	First axis-aligned bounding box reference

\param[in] vel1 (const AEVec2 &)
	First velocity vector

\param[in] aabb2 (const AABB &)
	Second axis-aligned bounding box reference

\param[in] vel2 (const AEVec2 &)
	Second velocity vector

\param[out] firstTimeOfCollision (float &)
	First time of collision

\return bool
	Return if true if there is an intersection, and false if there is not
*/
/**************************************************************************/
bool CollisionIntersection_RectRect(const AABB& aabb1,            //Input
									const AEVec2& vel1,           //Input 
									const AABB& aabb2,            //Input 
									const AEVec2& vel2,           //Input
									float& firstTimeOfCollision); //Output: the calculated value of tFirst, must be returned here


#endif // CSD1130_COLLISION_H_