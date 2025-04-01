/******************************************************************************/
/*!
\file		NetworkGameState.cpp
\author
\par
\date
\brief		This file contains the definitions of functions for updating the
			current game state and leaderboard.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

// Main header
#include "NetworkGameState.h"

// definition for networked game state
std::mutex gameStateMutex;
NetworkGameState currentGameState;

// definition for networked leaderboard
std::mutex leaderboardMutex;
NetworkLeaderboard leaderboard;

// Test Case
// Set network object with identifier 100, position, velocity, and scale
//	SetNetworkObject(100, { 10, 20 }, { 30, 40 }, { 50, 60 });
//	
//	// Variables to hold the object data
//	AEVec2 pos, vel, sca;
//	
//	// Get the network object data for identifier 100
//	GetNetworkObject(100, pos, vel, sca);
//	
//	// Print the position, velocity, and scale
//	std::cout << "Position: (" << pos.x << ", " << pos.y << "), "
//			  << "Velocity: (" << vel.x << ", " << vel.y << "), "
//			  << "Scale: (" << sca.x << ", " << sca.y << ")"
//			  << std::endl;
bool SetNetworkObject(uint32_t identifier, AEVec2 const& position, AEVec2 const& velocity, AEVec2 const& scale)
{
	// Mutex lock for synchronisation
	std::lock_guard<std::mutex> lock(gameStateMutex);

	// Set network data if it exist
	for (uint32_t x = 0; x < currentGameState.objectCount; ++x)
	{
		if (currentGameState.objects[x].identifier == identifier)
		{
			currentGameState.objects[x].transform.position = position;
			currentGameState.objects[x].transform.velocity = velocity;
			currentGameState.objects[x].transform.scale = scale;
			return true;
		}
	}

	// Add new network object to game state
	if (currentGameState.objectCount < MAX_NETWORK_OBJECTS)
	{
		uint32_t index = currentGameState.objectCount;
		currentGameState.objects[index].identifier = identifier;
		currentGameState.objects[index].transform.position = position;
		currentGameState.objects[index].transform.velocity = velocity;
		currentGameState.objects[index].transform.scale = scale;
		++currentGameState.objectCount;
		return true;
	}

	// If it reaches here, it means either there are too many objects in the scene
	// in which you can increase the MAX_NETWORK_OBJECTS
	// or too many duplicates of the same network object with different identifiers are found
	std::cerr << "Maximum number of network objects is reached!" << std::endl;

	return false;
}

bool GetNetworkObject(uint32_t identifier, AEVec2& position, AEVec2& velocity, AEVec2& scale)
{
	// Mutex lock for synchronisation
	std::lock_guard<std::mutex> lock(gameStateMutex);

	// Update object data if it exist
	for (uint32_t x = 0; x < currentGameState.objectCount; ++x)
	{
		if (currentGameState.objects[x].identifier == identifier)
		{
			position = currentGameState.objects[x].transform.position;
			velocity = currentGameState.objects[x].transform.velocity;
			scale = currentGameState.objects[x].transform.scale;
			return true;
		}
	}

	// If it reaches here, it means the an object is destroyed on the client
	// side but the server still has a copy of it
	std::cerr << "Error: Object [" << identifier << "] is not synchronised with the server!" << std::endl;

	return false;
}

bool SetNetworkPlayerData(uint32_t identifier, uint32_t score, uint32_t lives)
{
	// Mutex lock for synchronisation
	std::lock_guard<std::mutex> lock(gameStateMutex);

	// Set network player data if it exist
	for (uint32_t x = 0; x < currentGameState.playerCount; ++x)
	{
		if (currentGameState.playerData[x].identifier == identifier)
		{
			currentGameState.playerData[x].score = score;
			currentGameState.playerData[x].lives = lives;
			return true;
		}
	}

	// If player not found, add new if there's space
	if (currentGameState.playerCount < MAX_PLAYERS)
	{
		uint32_t index = currentGameState.playerCount++;
		currentGameState.playerData[index].identifier = identifier;
		currentGameState.playerData[index].score = score;
		currentGameState.playerData[index].lives = lives;
		return true;
	}

	return false;
}

bool GetNetworkPlayerData(uint32_t identifier, uint32_t& score, uint32_t& lives)
{
	// Mutex lock for synchronisation
	std::lock_guard<std::mutex> lock(gameStateMutex);

	// Update player data if it exist
	for (uint32_t x = 0; x < currentGameState.playerCount; ++x)
	{
		if (currentGameState.playerData[x].identifier == identifier)
		{
			score = currentGameState.playerData[x].score;
			lives = currentGameState.playerData[x].lives;
			return true;
		}
	}

	return false;
}

// Test Case
// // Can rerun and change values to constantly add new scores to the lsit
// LoadLeaderboard();
// AddScoreToLeaderboard(0, "Test", 1000, "PLACEHOLDER");
// AddScoreToLeaderboard(1, "Test2", 2000, "PLACEHOLDER");
// SaveLeaderboard();

bool AddScoreToLeaderboard(uint32_t identifier, char const* name, uint32_t score, char const* timestamp)
{
	// Mutex lock for synchronisation
	std::lock_guard<std::mutex> lock(leaderboardMutex);  // Ensures safe access

	// Add new score to leaderboard
	if (leaderboard.scoreCount < MAX_LEADERBOARD_SCORES)
	{
		NetworkScore& entry = leaderboard.scores[leaderboard.scoreCount++];

		entry.identifier = identifier;
		strncpy_s(entry.name, name, MAX_NAME_LENGTH);
		entry.score = score;
		strncpy_s(entry.timestamp, timestamp, TIME_FORMAT);

		// Sort the leaderboard
		std::sort(leaderboard.scores, leaderboard.scores + leaderboard.scoreCount,
			[](NetworkScore const& lhs, NetworkScore const& rhs)
			{
				return lhs.score > rhs.score;
			});

		return true;
	}
	else
	{
		// Find the lowest score on the leaderboard
		NetworkScore& entry = leaderboard.scores[MAX_LEADERBOARD_SCORES - 1];

		// Replace the score if the new score is higher
		if (score > entry.score)
		{
			entry.identifier = identifier;
			strncpy_s(entry.name, name, MAX_NAME_LENGTH);
			entry.score = score;
			strncpy_s(entry.timestamp, timestamp, TIME_FORMAT);

			// Sort the leaderboard
			std::sort(leaderboard.scores, leaderboard.scores + leaderboard.scoreCount,
				[](NetworkScore const& lhs, NetworkScore const& rhs)
				{
					return lhs.score > rhs.score;
				});

			return true;
		}
	}

	// Failed to break into the top MAX_LEADERBOARD_SCORES score on the leaderboard
	return false;
}

void SaveLeaderboard(char const* filename)
{
	// Mutex lock for synchronisation
	std::lock_guard<std::mutex> lock(leaderboardMutex);

	// Simple binary writer / reader
	std::ofstream file(filename, std::ios::binary);
	if (file) file.write(reinterpret_cast<char const*>(&leaderboard), sizeof(NetworkLeaderboard));
	file.close();
}

void LoadLeaderboard(char const* filename)
{
	// Mutex lock for synchronisation
	std::lock_guard<std::mutex> lock(leaderboardMutex);

	// Simple binary writer / reader
	std::ifstream file(filename, std::ios::binary);
	if (file) file.read(reinterpret_cast<char*>(&leaderboard), sizeof(NetworkLeaderboard));
	file.close();
	std::cout << leaderboard.scoreCount << std::endl;
}

std::vector<std::string> GetTopPlayersFromLeaderboard(uint32_t playerCount)
{
	// Mutex lock for synchronisation
	std::lock_guard<std::mutex> lock(leaderboardMutex);

	// Vector containing the players and their scores
	std::vector<std::string> topScores;

	// The number scores that could be print
	uint32_t count = (playerCount < leaderboard.scoreCount) ? playerCount : leaderboard.scoreCount;

	// Loop through the leaderboard
	// If the scores are out of order, can consider sorting it first before printing
	for (uint32_t x = 0; x < count; ++x)
	{
		NetworkScore const& score = leaderboard.scores[x];

		// Format string for printing
		std::ostringstream oss;
		oss << (x + 1) << ") " << score.name << ": " << score.score << " [" << score.timestamp << "]";
		topScores.push_back(oss.str());
	}

	return topScores;
}

// Test Case
//	AEVec2 playerPos = { 10.0f, 5.0f };
//	AEVec2 playerVel = { 1.0f, 0.5f };
//	
//	AEVec2 serverPos = { 12.0f, 6.0f };
//	AEVec2 serverVel = { 1.2f, 0.4f };
//	
//	for (int x = 0; x < 10; ++x)
//	{
//		// Server side
//		serverPos.x += serverVel.x;
//		serverPos.y += serverVel.y;
//	
//		// Client side
//		playerPos.x += playerVel.x;
//		playerPos.y += playerVel.y;
//	
//		std::cout << "Current:" << playerPos.x << ',' << playerPos.y << std::endl;
//	
//		// Apply correction for client
//		ApplySmoothCorrection(playerPos, serverPos);
//		ApplySmoothCorrection(playerVel, serverVel);
//	
//		// Results
//		std::cout << "Corrected:" << playerPos.x << ',' << playerPos.y << std::endl;
//		std::cout << "Expected:" << serverPos.x << ',' << serverPos.y << std::endl;
//	}
//	
//	// Final results
//	std::cout << "================================" << std::endl;
//	std::cout << "Current:" << playerPos.x << ',' << playerPos.y << std::endl;
//	std::cout << "Expected:" << serverPos.x << ',' << serverPos.y << std::endl;
//	std::cout << "================================" << std::endl;
void ApplySmoothCorrection(AEVec2& current, AEVec2 expected, float smoothingFactor, float maxCorrection)
{
	// Calculated correction required to match expected
	AEVec2 correction = AEVec2{ (expected.x - current.x) * smoothingFactor, (expected.y - current.y) * smoothingFactor };

	// Clamp correction
	correction.x = (correction.x < -maxCorrection) ? -maxCorrection : (correction.x > maxCorrection) ? maxCorrection : correction.x;
	correction.y = (correction.y < -maxCorrection) ? -maxCorrection : (correction.y > maxCorrection) ? maxCorrection : correction.y;

	// Apply correction
	current.x += correction.x;
	current.y += correction.y;
}