/******************************************************************************/
/*!
\file		Collision.cpp
\author 	
\par    	
\date   	
\brief		This file contains the definition for the AABB rectangle to rectangle
collision function.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#include "main.h"

/**************************************************************************/
/*!

	*/
/**************************************************************************/
bool CollisionIntersection_RectRect(const AABB & aabb1,          //Input
									const AEVec2 & vel1,         //Input 
									const AABB & aabb2,          //Input 
									const AEVec2 & vel2,         //Input
									float& firstTimeOfCollision) //Output: the calculated value of tFirst, below, must be returned here
{
	UNREFERENCED_PARAMETER(aabb1);
	UNREFERENCED_PARAMETER(vel1);
	UNREFERENCED_PARAMETER(aabb2);
	UNREFERENCED_PARAMETER(vel2);
	UNREFERENCED_PARAMETER(firstTimeOfCollision);

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

	// static collision between rectangles
	if (aabb1.max.x < aabb2.min.x ||
		aabb1.min.x > aabb2.max.x ||
		aabb1.max.y < aabb2.min.y ||
		aabb1.min.y > aabb2.max.y) {
		// there is no static collision ... proceed with dynamic collision test

		// step 2: init and calc new velocity
		float tFirst = 0.0f, tLast = g_dt;
		AEVec2 vA = vel1;
		AEVec2 vB = vel2;

		vB.x = vB.x - vA.x;
		vB.y = vB.y - vA.y;

		// step 3: working with x axis
		if (vB.x < 0.f) { 

			// case 1: move away
			if (aabb1.min.x > aabb2.max.x) { 
				return false; // no intersection
			}

			// case 4: move towards
			if (aabb1.max.x < aabb2.min.x) {
				tFirst = max(tFirst, (aabb1.max.x - aabb2.min.x) / vB.x);
			}
			if (aabb1.min.x < aabb2.max.x) {
				tLast = min(tLast, (aabb1.min.x - aabb2.max.x) / vB.x);
			}

		} else if (vB.x > 0.f) { 

			// case 2
			if (aabb1.min.x > aabb2.max.x) {
				tFirst = max(tFirst,(aabb1.min.x - aabb2.max.x) / vB.x);
			}
			if (aabb1.max.x > aabb2.min.x) {
				tLast = min(tLast, (aabb1.max.x - aabb2.min.x) / vB.x);
			}
			
			// case 3: move away
			if (aabb1.max.x < aabb2.min.x) {
				return false; // no intersection
			}

		} else {
			// case 5: vB == 0
			if (aabb1.max.x < aabb2.min.x) {
				return false;
			}
			else if (aabb1.min.x > aabb2.max.x) {
				return false;
			}
		}

		if (tFirst > tLast) {
			return false; // no intersection
		}

		// step 4: working with y axis
		if (vB.y < 0) {

			// case 1: move away
			if (aabb1.min.y > aabb2.max.y) {
				return false; // no intersection
			}

			// case 4: move towards
			if (aabb1.max.y < aabb2.min.y) {
				tFirst = max(tFirst, (aabb1.max.y - aabb2.min.y) / vB.y);
			}
			if (aabb1.min.y < aabb2.max.y) {
				tLast = min(tLast, (aabb1.min.y - aabb2.max.y) / vB.y);
			}

		} else if (vB.y > 0) {

			// case 2

			if (aabb1.min.y > aabb2.max.y) {
				tFirst = max(tFirst, (aabb1.min.y - aabb2.max.y) / vB.y);
			}
			if (aabb1.max.y > aabb2.min.y) {
				tLast = min(tLast, (aabb1.max.y - aabb2.min.y) / vB.y);
			}

			// case 3: move away
			if (aabb1.max.y < aabb2.min.y) {
				return false; // no intersection
			}

		} else {
			// case 5: vB == 0
			if (aabb1.max.y < aabb2.min.y) {
				return false; // no intersection
			}
			else if (aabb1.min.y > aabb2.max.y) {
				return false; // no intersection
			}
		}

		if (tFirst > tLast) {
			return false; // no intersection
		}

		firstTimeOfCollision = tFirst;

	}
	
	// step 5: rects intersect
	return true;
}