#include "rainbow.h"

void Rainbow::CycleRainbow(int ServerTick, CPlayer *m_pPlayer, int Hue, int Sat, int Lht, int FEET_OFFSET, RainbowType Type, int MaxValue, bool ReverseRainbow, int SpeedMultiplier, int RangeMultiplier)
{
	auto Calc = [](int SvTick, int Min, int Max, bool Reverse = false, int SpeedMult = 1, int RangeMult = 2)
	{
		const int Range = Max - Min;
		const int Tick = (SvTick * SpeedMult) % (2 * Range);
		const int State = Tick >= Range;
		const int Color = Reverse ? (Max - (State ? (2 * Range - 1 - Tick) : Tick)) : (Min + (State ? (2 * Range - 1 - Tick) : Tick));
		return Color;
	};

	if (Type == HUE)
	{
		m_pPlayer->m_TeeInfos.m_ColorBody =
			(
				Calc(ServerTick, Hue, MaxValue, ReverseRainbow, SpeedMultiplier, RangeMultiplier) << 0x10
				) | ((Sat << 0x8) | Lht);

		m_pPlayer->m_TeeInfos.m_ColorFeet =
			(
				Calc(ServerTick + FEET_OFFSET, Hue, MaxValue, ReverseRainbow, SpeedMultiplier, RangeMultiplier) << 0x10
				) | ((Sat << 0x8) | Lht);
	}
	else if (Type == SAT)
	{
		m_pPlayer->m_TeeInfos.m_ColorBody =
			(
				Hue << 0x10
				) | ((Calc(ServerTick, Sat, MaxValue, ReverseRainbow, SpeedMultiplier, RangeMultiplier) << 0x8) | Lht);

		m_pPlayer->m_TeeInfos.m_ColorFeet =
			(
				Hue << 0x10
				) | ((Calc(ServerTick + FEET_OFFSET, Sat, MaxValue, ReverseRainbow, SpeedMultiplier, RangeMultiplier) << 0x8) | Lht);
	}
	else if (Type == LHT)
	{
		m_pPlayer->m_TeeInfos.m_ColorBody =
			(
				Hue << 0x10
				) | ((Sat << 0x8) | (Calc(ServerTick, Lht, MaxValue, ReverseRainbow, SpeedMultiplier, RangeMultiplier)));

		m_pPlayer->m_TeeInfos.m_ColorFeet =
			(
				Hue << 0x10
				) | ((Sat << 0x8) | (Calc(ServerTick + FEET_OFFSET, Lht, MaxValue, ReverseRainbow, SpeedMultiplier, RangeMultiplier)));
	}
}