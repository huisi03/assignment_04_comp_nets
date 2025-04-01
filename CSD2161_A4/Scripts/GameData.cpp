// Includes
#include "GameData.h"

// Globals
std::mutex gameDataMutex;

std::unordered_map<uint16_t, PlayerInput> playerInputMap;
std::map<uint16_t, PlayerData> playerDataMap;
std::vector<NetworkTransform> asteroids;
std::unordered_map<uint16_t, std::vector<NetworkTransform>> playerBulletMap;
PlayerData clientData;
NetworkGameState gameDataState;

