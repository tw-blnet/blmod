#include "rainbow.h"

void Rainbow::CycleRainbow(int ServerTick, CPlayer *m_pPlayer, int Hue, int Sat, int Lht, RainbowType Type, int MaxValue, int FEET_OFFSET, int SpeedMultiplier, int RangeMultiplier)
{
	auto Calc = [](int SvTick, int Min, int Max, int SpeedMult = 1, int RangeMult = 2)
	{
		const int Range = abs(Max - Min);
		const int Tick = (SvTick * SpeedMult) % (2 * Range);
		const int State = Tick >= Range;
		const int Color = (Min>Max) ? (Min - (State ? (2 * Range - 1 - Tick) : Tick)) : (Min + (State ? (2 * Range - 1 - Tick) : Tick));
		return Color;
	};

	if (Type == HUE)
	{
		if (MaxValue == Hue)
		{
			return;
		}
		m_pPlayer->m_TeeInfos.m_ColorBody =
			(
				Calc(ServerTick, Hue, MaxValue, SpeedMultiplier, RangeMultiplier) << 0x10
				) | ((Sat << 0x8) | Lht);

		m_pPlayer->m_TeeInfos.m_ColorFeet =
			(
				Calc(ServerTick + FEET_OFFSET, Hue, MaxValue, SpeedMultiplier, RangeMultiplier) << 0x10
				) | ((Sat << 0x8) | Lht);
	}
	else if (Type == SAT)
	{
		if (MaxValue == Sat)
		{
			return;
		}
		m_pPlayer->m_TeeInfos.m_ColorBody =
			(
				Hue << 0x10
				) | ((Calc(ServerTick, Sat, MaxValue, SpeedMultiplier, RangeMultiplier) << 0x8) | Lht);

		m_pPlayer->m_TeeInfos.m_ColorFeet =
			(
				Hue << 0x10
				) | ((Calc(ServerTick + FEET_OFFSET, Sat, MaxValue, SpeedMultiplier, RangeMultiplier) << 0x8) | Lht);
	}
	else if (Type == LHT)
	{
		if (MaxValue == Lht)
		{
			return;
		}
		m_pPlayer->m_TeeInfos.m_ColorBody =
			(
				Hue << 0x10
				) | ((Sat << 0x8) | (Calc(ServerTick, Lht, MaxValue, SpeedMultiplier, RangeMultiplier)));

		m_pPlayer->m_TeeInfos.m_ColorFeet =
			(
				Hue << 0x10
				) | ((Sat << 0x8) | (Calc(ServerTick + FEET_OFFSET, Lht, MaxValue, SpeedMultiplier, RangeMultiplier)));
	}
}