#ifndef GAME_SERVER_BLOCKTRACKER_H
#define GAME_SERVER_BLOCKTRACKER_H

#include <engine/shared/protocol.h>

class CGameContext;

class CBlockTracker
{
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
