#ifndef GAME_SERVER_TEEINFO_H
#define GAME_SERVER_TEEINFO_H

struct StdSkin
{
	char m_SkinName[64];
	char m_apSkinPartNames[6][24];
	bool m_aUseCustomColors[6];
	int m_aSkinPartColors[6];
};

extern StdSkin g_StdSkins[];
extern size_t g_StdSkinsCount;

class CTeeInfo
{
public:
    constexpr static const float DARKEST_LGT_7 = 100/255.0f;

    char m_SkinName[64] = {'\0'};
    int m_UseCustomColor = 0;
    int m_ColorBody = 0;
    int m_ColorFeet = 0;

    // 0.7
    char m_apSkinPartNames[6][24] = {"", "", "", "", "", ""};
    bool m_aUseCustomColors[6] = {false, false, false, false, false, false};
    int m_aSkinPartColors[6] = {0, 0, 0, 0, 0, 0};

    CTeeInfo() = default;

    CTeeInfo(const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet);

    // This constructor will assume all arrays are of length 6
    CTeeInfo(const char *pSkinPartNames[6], int *pUseCustomColors, int *pSkinPartColors);

    void FromSixup();
    void ToSixup();

    // return hash for skin color
    unsigned long colorHash();

    // return hash for skin color & skin name
    unsigned long fullHash();
};
#endif //GAME_SERVER_TEEINFO_H
