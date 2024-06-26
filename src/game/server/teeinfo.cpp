#include <base/system.h>
#include <base/color.h>
#include <zlib.h>

#include "teeinfo.h"

StdSkin g_StdSkins[] = {
{"default",{"standard","","","standard","standard","standard"},{true,false,false,true,true,false},{1798004,0,0,1799582,1869630,0}},
{"bluekitty",{"kitty","whisker","","standard","standard","standard"},{true,true,false,true,true,false},{8681144,-8229413,0,7885547,7885547,0}},
{"bluestripe",{"standard","stripes","","standard","standard","standard"},{true,false,false,true,true,false},{10187898,0,0,750848,1944919,0}},
{"brownbear",{"bear","bear","hair","standard","standard","standard"},{true,true,false,true,true,false},{1082745,-15634776,0,1082745,1147174,0}},
{"cammo",{"standard","cammo2","","standard","standard","standard"},{true,true,false,true,true,false},{5334342,-11771603,0,750848,1944919,0}},
{"cammostripes",{"standard","cammostripes","","standard","standard","standard"},{true,true,false,true,true,false},{5334342,-14840320,0,750848,1944919,0}},
{"coala",{"koala","twinbelly","","standard","standard","standard"},{true,true,false,true,true,false},{184,-15397662,0,184,9765959,0}},
{"limekitty",{"kitty","whisker","","standard","standard","standard"},{true,true,false,true,true,false},{4612803,-12229920,0,3827951,3827951,0}},
{"pinky",{"standard","whisker","","standard","standard","standard"},{true,true,false,true,true,false},{15911355,-801066,0,15043034,15043034,0}},
{"redbopp",{"standard","donny","unibop","standard","standard","standard"},{true,true,true,true,true,false},{16177260,-16590390,16177260,16177260,7624169,0}},
{"redstripe",{"standard","stripe","","standard","standard","standard"},{true,false,false,true,true,false},{16307835,0,0,184,9765959,0}},
{"saddo",{"standard","saddo","","standard","standard","standard"},{true,true,false,true,true,false},{7171455,-9685436,0,3640746,5792119,0}},
{"toptri",{"standard","toptri","","standard","standard","standard"},{true,false,false,true,true,false},{6119331,0,0,3640746,5792119,0}},
{"twinbop",{"standard","duodonny","twinbopp","standard","standard","standard"},{true,true,true,true,true,false},{15310519,-1600806,15310519,15310519,37600,0}},
{"twintri",{"standard","twintri","","standard","standard","standard"},{true,true,false,true,true,false},{3447932,-14098717,0,185,9634888,0}},
{"warpaint",{"standard","warpaint","","standard","standard","standard"},{true,false,false,true,true,false},{1944919,0,0,750337,1944919,0}}};

size_t g_StdSkinsCount = sizeof(g_StdSkins) / sizeof(StdSkin);

CTeeInfo::CTeeInfo(const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet)
{
	str_copy(m_SkinName, pSkinName, sizeof(m_SkinName));
	m_UseCustomColor = UseCustomColor;
	m_ColorBody = ColorBody;
	m_ColorFeet = ColorFeet;
}

CTeeInfo::CTeeInfo(const char *pSkinPartNames[6], int *pUseCustomColors, int *pSkinPartColors)
{
	for(int i = 0; i < 6; i++)
	{
		str_copy(m_apSkinPartNames[i], pSkinPartNames[i], sizeof(m_apSkinPartNames[i]));
		m_aUseCustomColors[i] = pUseCustomColors[i];
		m_aSkinPartColors[i] = pSkinPartColors[i];
	}
}

void CTeeInfo::ToSixup()
{
	// reset to default skin
	for(int p = 0; p < 6; p++)
	{
		str_copy(m_apSkinPartNames[p], g_StdSkins[0].m_apSkinPartNames[p], 24);
		m_aUseCustomColors[p] = g_StdSkins[0].m_aUseCustomColors[p];
		m_aSkinPartColors[p] = g_StdSkins[0].m_aSkinPartColors[p];
	}

	// check for std skin
	for(int s = 0; s < 16; s++)
	{
		if(!str_comp(m_SkinName, g_StdSkins[s].m_SkinName))
		{
			for(int p = 0; p < 6; p++)
			{
				str_copy(m_apSkinPartNames[p], g_StdSkins[s].m_apSkinPartNames[p], 24);
				m_aUseCustomColors[p] = g_StdSkins[s].m_aUseCustomColors[p];
				m_aSkinPartColors[p] = g_StdSkins[s].m_aSkinPartColors[p];
			}
			break;
		}
	}

	if(m_UseCustomColor)
	{
		int ColorBody = ColorHSLA(m_ColorBody).UnclampLighting().Pack(DARKEST_LGT_7);
		int ColorFeet = ColorHSLA(m_ColorFeet).UnclampLighting().Pack(DARKEST_LGT_7);
		m_aUseCustomColors[0] = true;
		m_aUseCustomColors[1] = true;
		m_aUseCustomColors[2] = true;
		m_aUseCustomColors[3] = true;
		m_aUseCustomColors[4] = true;
		m_aSkinPartColors[0] = ColorBody;
		m_aSkinPartColors[1] = 0x22FFFFFF;
		m_aSkinPartColors[2] = ColorBody;
		m_aSkinPartColors[3] = ColorBody;
		m_aSkinPartColors[4] = ColorFeet;
	}
}

void CTeeInfo::FromSixup()
{
	// reset to default skin
	str_copy(m_SkinName, "default", sizeof(m_SkinName));
	m_UseCustomColor = false;
	m_ColorBody = 0;
	m_ColorFeet = 0;

	// check for std skin
	for(int s = 0; s < 16; s++)
	{
		bool match = true;
		for(int p = 0; p < 6; p++)
		{
			if(str_comp(m_apSkinPartNames[p], g_StdSkins[s].m_apSkinPartNames[p])
					|| m_aUseCustomColors[p] != g_StdSkins[s].m_aUseCustomColors[p]
					|| (m_aUseCustomColors[p] && m_aSkinPartColors[p] != g_StdSkins[s].m_aSkinPartColors[p]))
			{
				match = false;
				break;
			}
		}
		if(match)
		{
			str_copy(m_SkinName, g_StdSkins[s].m_SkinName, sizeof(m_SkinName));
			return;
		}
	}

	// find closest match
	int best_skin = 0;
	int best_matches = -1;
	for(int s = 0; s < 16; s++)
	{
		int matches = 0;
		for(int p = 0; p < 3; p++)
			if(str_comp(m_apSkinPartNames[p], g_StdSkins[s].m_apSkinPartNames[p]) == 0)
				matches++;

		if(matches > best_matches)
		{
			best_matches = matches;
			best_skin = s;
		}
	}

	str_copy(m_SkinName, g_StdSkins[best_skin].m_SkinName, sizeof(m_SkinName));
	m_UseCustomColor = true;
	m_ColorBody = ColorHSLA(m_aUseCustomColors[0] ? m_aSkinPartColors[0] : 255).UnclampLighting(DARKEST_LGT_7).Pack(ColorHSLA::DARKEST_LGT);
	m_ColorFeet = ColorHSLA(m_aUseCustomColors[4] ? m_aSkinPartColors[4] : 255).UnclampLighting(DARKEST_LGT_7).Pack(ColorHSLA::DARKEST_LGT);
}

unsigned long CTeeInfo::colorHash()
{
	uLong crc = crc32(0L, Z_NULL, 0);

	crc = crc32(crc, (Bytef*) &m_UseCustomColor, sizeof(m_UseCustomColor));
	crc = crc32(crc, (Bytef*) &m_ColorBody, sizeof(m_ColorBody));
	crc = crc32(crc, (Bytef*) &m_ColorFeet, sizeof(m_ColorFeet));
	crc = crc32(crc, (Bytef*) &m_aUseCustomColors, sizeof(m_aUseCustomColors));
	crc = crc32(crc, (Bytef*) &m_aSkinPartColors, sizeof(m_aSkinPartColors));

	return crc;
}

unsigned long CTeeInfo::fullHash()
{
	uLong crc = colorHash();

	crc = crc32(crc, (Bytef*) &m_SkinName, sizeof(m_SkinName));
	crc = crc32(crc, (Bytef*) &m_apSkinPartNames, sizeof(m_apSkinPartNames));

	return crc;
}
