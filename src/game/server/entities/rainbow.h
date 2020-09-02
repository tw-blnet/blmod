#pragma once
#include <game/server/player.h>

enum RainbowType
{
	HUE,
	SAT,
	LHT,
};

class Rainbow
{
public:
	static void CycleRainbow(int ServerTick, CPlayer * m_pPlayer, int Hue, int Sat, int Lht, RainbowType Type, int MaxValue, int FEET_OFFSET = 0, int SpeedMultiplier = 1, int RangeMultiplier = 2);
};

