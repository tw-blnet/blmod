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
	static void CycleRainbow(int ServerTick, CPlayer * m_pPlayer, int Hue, int Sat, int Lht, int OffsetFeet, RainbowType Type, int MaxValue, bool ReverseRainbow = false, int SpeedMultiplier = 1, int RangeMultiplier = 2);
};

