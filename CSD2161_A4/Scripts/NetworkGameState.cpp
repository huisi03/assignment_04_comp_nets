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

void ClearNetworkData()
{
	std::lock_guard<std::mutex> lock(gameStateMutex);
	currentGameState.sequenceNumber = 0;
	currentGameState.playerCount = 0;
	currentGameState.objectCount = 0;
}

bool AddNetworkPlayerData(uint32_t identifier, uint32_t score, uint32_t lives)
{
	std::lock_guard<std::mutex> lock(gameStateMutex);

	if (currentGameState.playerCount >= MAX_PLAYERS)
	{
		// No space left
		return false; 
	}

	NetworkPlayerData& newPlayer = currentGameState.playerData[currentGameState.playerCount];
	newPlayer.identifier = identifier;
	newPlayer.score = score;
	newPlayer.lives = lives;

	++currentGameState.playerCount;
	return true;
}

bool AddNetworkObject(int type, AEVec2 const& position, AEVec2 const& velocity, float rotation, AEVec2 const& scale)
{
	std::lock_guard<std::mutex> lock(gameStateMutex);

	if (currentGameState.objectCount >= MAX_NETWORK_OBJECTS)
	{
		// No space left, increase MAX_NETWORK_OBJECTS if necessary
		return false; 
	}

	NetworkObject& newObject = currentGameState.objects[currentGameState.objectCount];
	newObject.type = type;
	newObject.transform.position = position;
	newObject.transform.velocity = velocity;
	newObject.transform.rotation = rotation;
	newObject.transform.scale = scale;

	++currentGameState.objectCount;

	return true;
}

NetworkPlayerData GetNetworkPlayerData(uint32_t identifier)
{
	std::lock_guard<std::mutex> lock(gameStateMutex);

	for (uint32_t x = 0; x < currentGameState.playerCount; ++x)
	{
		if (currentGameState.playerData[x].identifier == identifier)
		{
			return currentGameState.playerData[x];
		}
	}

	// return empty
	return NetworkPlayerData();
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
	ApplySmoothCorrection(current.x, expected.x, smoothingFactor, maxCorrection); 
	ApplySmoothCorrection(current.y, expected.y, smoothingFactor, maxCorrection);
}

void ApplySmoothCorrection(float& current, float expected, float smoothingFactor, float maxCorrection)
{
	// Calculated correction required to match expected
	float correction = (expected - current) * smoothingFactor;

	// Clamp correction
	correction = (correction < -maxCorrection) ? -maxCorrection : (correction > maxCorrection) ? maxCorrection : correction;

	// Apply correction
	current += correction;
}