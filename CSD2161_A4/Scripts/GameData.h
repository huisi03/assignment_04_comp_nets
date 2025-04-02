#ifndef GAME_DATA
#define GAME_DATA

#include <unordered_map>    // unordered map
#include <map>
#include "NetworkGameState.h"

struct PlayerInput
{
	bool upKey = false;
	bool downKey = false;
	bool rightKey = false;
	bool leftKey = false;
	bool spaceKey = false;
	void NoInput()
	{
		upKey = downKey = rightKey = leftKey = spaceKey = false;
	}
};

struct PlayerData
{
	PlayerData() = default;
	PlayerData(AEVec2 pos, AEVec2 scale)
		: transform{ pos, {0, 0}, 0.0f, scale }, stats{} {}
	NetworkTransform transform;
	NetworkPlayerData stats;
};

// externs
extern std::unordered_map<uint16_t, PlayerInput> playerInputMap;
extern std::map<uint16_t, PlayerData> playerDataMap;
extern std::vector<NetworkTransform> asteroids;
extern std::unordered_map<uint16_t, std::vector<NetworkTransform>> playerBulletMap;
extern std::mutex gameDataMutex;
extern PlayerData clientData;
extern NetworkGameState gameDataState;










#endif
