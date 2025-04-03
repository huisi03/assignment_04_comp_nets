// Includes
#include "GameData.h"

// Globals
std::mutex gameDataMutex;

std::unordered_map<uint16_t, PlayerInput> playerInputMap;						// A map containing the player portID against the player input
std::map<uint16_t, PlayerData> playerDataMap;									// Data on server side for all players data
std::vector<NetworkTransform> asteroids;										// Vector of asteroids
std::unordered_map<uint16_t, std::vector<NetworkTransform>> playerBulletMap;	// A map containing the player portID against the bullets to track which bullets belong to which player
PlayerData clientData;															// Data on client side for player's own data
NetworkGameState gameDataState;													// Data on both side that will contain all game object

std::map<uint16_t, bool> isPlayerConnected;										// portID -> true/false
std::map<uint16_t, uint64_t> lastHeardTime;										// portID -> last packet time (ms)