#include "NameGenerator.h"

namespace
{
	std::array<char[4], 26> names =
	{
		"ACE", "BOB", "CHA", "DAN", "EVE", "FAN",
		"GUS", "HAL", "IAN", "JAY", "KEN", "LEO",
		"MAX", "NED", "OLL", "PAZ", "QUY", "RON",
		"SAM", "TOM", "UDO", "VIN", "WES", "XAV",
		"YEN", "ZED"
	};
}

std::string RandomiseName()
{
	static f64 time = 0;
	if (time == 0) AEGetTime(&time);
	static std::default_random_engine dre(static_cast<unsigned>(time * 1000));
	static std::uniform_int_distribution<size_t> dist(0, names.size() - 1);
	return names[dist(dre)];
}