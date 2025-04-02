/******************************************************************************/
/*!
\file		NetworkGameState.h
\author
\par
\date
\brief		This file declares the functions and structures used for managing 
			the networked game state, including object synchronisation 
			and leaderboard handling.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#include <iostream>					// std::cout, uint32_t
#include <AEEngine.h>				// AEVec2
#include <vector>					// std::vector
#include <mutex>					// std::mutex
#include <algorithm>				// std::sort
#include <fstream>					// std::ifstream, std::ofstream
#include <sstream>					// std::osstream

#pragma pack(1)
// Struct to hold the transformation data of a networked object
struct NetworkTransform
{
	AEVec2 position;				// position of the object
	AEVec2 velocity;				// velocity of the object
	AEVec2 scale;					// scale of the object (should rarely be changed)
};

// Struct to represent a networked object with an identifier and transformation data
struct NetworkObject
{
	uint32_t identifier;			// unique object id
	NetworkTransform transform;		// object transform
};

// Struct to represent player data in the game
struct NetworkPlayerData
{
	uint32_t identifier;			// unique player id
	uint32_t score;					// player score
	uint32_t lives;					// number of lives left
};

#define MAX_NAME_LENGTH			8	// Maximum length of a name
#define TIME_FORMAT				20	// Format: "YYYY-MM-DD HH:MM:SS"
#define MAX_LEADERBOARD_SCORES	20	// Maximum number of scores allowed on the leaderboard
#define LEADERBOARD_FILE_NAME	"Resources/Leaderboard.txt"	// File name for the leaderboard

// Struct to represent a player's score and additional data for the leaderboard
struct NetworkScore
{
	uint32_t identifier;			// unique player id
	char name[MAX_NAME_LENGTH];		// player name
	uint32_t score;					// player score
	char timestamp[TIME_FORMAT];	// time stamp of the achieved score
};

// Struct to represent the leaderboard with a list of scores and the count
struct NetworkLeaderboard
{
	uint32_t scoreCount;
	NetworkScore scores[MAX_LEADERBOARD_SCORES];
};

#define MAX_NETWORK_OBJECTS		32		// maximum number of objects
#define MAX_PLAYERS				4		// maximum number of players in a lobby

// Main game state, including both player data and objects in the game world
struct NetworkGameState
{
	uint32_t sequenceNumber;

	uint32_t playerCount;
	NetworkPlayerData playerData[MAX_PLAYERS];

	uint32_t objectCount;
	NetworkObject objects[MAX_NETWORK_OBJECTS];
};
#pragma pack()

// Network game states (To be passed via UDP)
extern std::mutex gameStateMutex;
extern NetworkGameState currentGameState;

// Leaderboard saved on the network
extern std::mutex leaderboardMutex;
extern NetworkLeaderboard leaderboard;

// Function to set the position, velocity, and scale of a networked object
// Returns false if the object data could not be set
bool SetNetworkObject(uint32_t identifier, AEVec2 const& position, AEVec2 const& velocity, AEVec2 const& scale);

// Function to get the position, velocity, and scale of a networked object
// Returns false if the object data does not exist
bool GetNetworkObject(uint32_t identifier, AEVec2& position, AEVec2& velocity, AEVec2& scale);

// Function to set the player's score and lives data in the networked game state
// Returns false if the player data could not be set
bool SetNetworkPlayerData(uint32_t identifier, uint32_t score, uint32_t lives);

// Function to get the player's score and lives data from the networked game state
// Returns false if the player data does not exist
bool GetNetworkPlayerData(uint32_t identifier, uint32_t& score, uint32_t& lives);

// Function to add a new score to the leaderboard, replacing the lowest score if necessary
// Returns false if the score cannot be added (e.g., it's not high enough)
bool AddScoreToLeaderboard(uint32_t identifier, char const* name, uint32_t score, char const* timestamp);

// Function to save the current leaderboard to a file
void SaveLeaderboard(char const* filename = LEADERBOARD_FILE_NAME);

// Function to load the leaderboard from a file
void LoadLeaderboard(char const* filename = LEADERBOARD_FILE_NAME);

// Function to retrieve a list of the top players from the leaderboard, formatted as strings
// It returns a vector of strings, each containing a player's name, score, and timestamp
std::vector<std::string> GetTopPlayersFromLeaderboard(uint32_t playerCount = 5);

// Function to apply a correction of the object's current value with the expected value on
// the network side
void ApplySmoothCorrection(AEVec2& current, AEVec2 expected, float smoothingFactor = 0.3f, float maxCorrection = 0.5f);