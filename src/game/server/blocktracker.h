#ifndef GAME_SERVER_BLOCKTRACKER_H
#define GAME_SERVER_BLOCKTRACKER_H

#include <engine/shared/protocol.h>

class CGameContext;

class CBlockTracker
{
    const int EXPERIENCE_FOR_BLOCK = 1;
    const float FREEZED_TO_BLOCK_SECONDS = 4;
    const float UNFREEZED_TO_RESET_SECONDS = 3;
    const float NO_IMPACT_TO_RESET_SECONDS = 2;
    const float IMPACT_WINDOW_TO_RESIST_SECONDS = 5;
    const float NO_IMPACT_AFTER_UNFREEZE_SECONDS = 3;
    const float KILL_INTERVAL_SECONDS = 5;

    struct CTrackedPlayer
    {
        bool m_Tracked;
        bool m_IsResisted;
        int m_LastActionTick;
        int m_ImpactedClientID;
        int m_LastImpactedTick;
        int m_FreezedTick;
        int m_UnfreezedTick;
        int m_KilledTick;
    };

	class CGameContext *m_pGameContext;
    CTrackedPlayer m_aTrackedPlayers[MAX_CLIENTS];

    float SecondsPassed(int SinceTick);
    bool Blocked(int ClientID, int BlockerID);

public:
	CBlockTracker(class CGameContext *pGameServer);

	void Tick();

    void StartTrackPlayer(int ClientID);
    void StopTrackPlayer(int ClientID);

    void OnPlayerAction(int ClientID);
    void OnPlayerFreeze(int ClientID);
    void OnPlayerUnfreeze(int ClientID);
    void OnPlayerImpacted(int ClientID, int InitiatorID);
    bool OnPlayerKill(int ClientID);
};

#endif
