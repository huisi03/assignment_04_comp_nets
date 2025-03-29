/******************************************************************************/
/*!
\file		Collision.cpp
\author		
\par		
\date		
\brief		This file contains the definitions of functions to check for collisions
			between objects

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "Collision.h" // main headers

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
bool CollisionIntersection_RectRect(const AABB & aabb1,          //Input
									const AEVec2 & vel1,         //Input 
									const AABB & aabb2,          //Input 
									const AEVec2 & vel2,         //Input
									float& firstTimeOfCollision) //Output: the calculated value of tFirst, below, must be returned here
{
	/*
	Implement the collision intersection over here.

	The steps are:	
	Step 1: Check for static collision detection between rectangles (static: before moving). 
				If the check returns no overlap, you continue with the dynamic collision test
					with the following next steps 2 to 5 (dynamic: with velocities).
				Otherwise you return collision is true, and you stop.

	Step 2: Initialize and calculate the new velocity of Vb
			tFirst = 0  //tFirst variable is commonly used for both the x-axis and y-axis
			tLast = dt  //tLast variable is commonly used for both the x-axis and y-axis

	Step 3: Working with one dimension (x-axis).
			if(Vb < 0)
				case 1
				case 4
			else if(Vb > 0)
				case 2
				case 3
			else //(Vb == 0)
				case 5

			case 6

	Step 4: Repeat step 3 on the y-axis

	Step 5: Return true: the rectangles intersect

	*/
	if (aabb1.max.x > aabb2.min.x && aabb1.max.y > aabb2.min.y &&
		aabb2.max.x > aabb1.min.x && aabb2.max.y > aabb1.min.y)
	{
		return true;
	}
	else
	{
		f32 tFirst = 0;
		f32 tLast = (f32)AEFrameRateControllerGetFrameTime();

		AEVec2 vb;
		AEVec2Set(&vb, vel2.x - vel1.x, vel2.y - vel1.y);

		// check via x
		if (vb.x < 0)
		{
			// case 1
			if (aabb1.min.x > aabb2.max.x) return false;

			// case 4
			if (aabb1.max.x < aabb2.min.x)
			{
				f32 dFirst = aabb1.max.x - aabb2.min.x;
				tFirst = max(dFirst / vb.x, tFirst);
			}

			if (aabb1.min.x < aabb2.max.x)
			{
				f32 dLast = aabb1.min.x - aabb2.max.x;
				tLast = min(dLast / vb.x, tLast);
			}
		}
		else if (vb.x > 0)
		{
			// case 3
			if (aabb1.max.x < aabb2.min.x) return false;

			// case 2
			if (aabb1.min.x > aabb2.max.x)
			{
				f32 dFirst = aabb1.min.x - aabb2.max.x;
				tFirst = max(dFirst / vb.x, tFirst);
			}

			if (aabb1.max.x > aabb2.min.x)
			{
				f32 dLast = aabb1.max.x - aabb2.min.x;
				tLast = min(dLast / vb.x, tLast);
			}
		}
		else
		{
			// case 6
			if (aabb1.max.x < aabb2.min.x) return false;
			else if (aabb1.min.x > aabb2.max.x) return false;
		}

		if (tFirst > tLast) return false;

		// repeat for y
		if (vb.y < 0)
		{
			// case 1
			if (aabb1.min.y > aabb2.max.y) return false;

			// case 4
			if (aabb1.max.y < aabb2.min.y)
			{
				f32 dFirst = aabb1.max.y - aabb2.min.y;
				tFirst = max(dFirst / vb.y, tFirst);
			}

			if (aabb1.min.y < aabb2.max.y)
			{
				f32 dLast = aabb1.min.y - aabb2.max.y;
				tLast = min(dLast / vb.y, tLast);
			}
		}
		else if (vb.y > 0)
		{
			// case 3
			if (aabb1.max.y < aabb2.min.y) return false;

			// case 2
			if (aabb1.min.y > aabb2.max.y)
			{
				f32 dFirst = aabb1.min.y - aabb2.max.y;
				tFirst = max(dFirst / vb.y, tFirst);
			}

			if (aabb1.max.y > aabb2.min.y)
			{
				f32 dLast = aabb1.max.y - aabb2.min.y;
				tLast = min(dLast / vb.y, tLast);
			}
		}
		else
		{
			// case 6
			if (aabb1.max.y < aabb2.min.y) return false;
			else if (aabb1.min.y > aabb2.max.y) return false;
		}

		if (tFirst > tLast) return false;

		// set first time of collision
		firstTimeOfCollision = tFirst;

		return true;
	}
}