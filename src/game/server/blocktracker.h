#ifndef GAME_SERVER_BLOCKTRACKER_H
#define GAME_SERVER_BLOCKTRACKER_H

#include <engine/shared/protocol.h>

class CGameContext;

class CBlockTracker
{
    struct CTrackedPlayer
    {
        bool m_Tracked;
    };

	class CGameContext *m_pGameContext;
    CTrackedPlayer m_aTrackedPlayers[MAX_CLIENTS];

public:
	CBlockTracker(class CGameContext *pGameServer);

	void Tick();

    void StartTrackPlayer(int ClientID);
    void StopTrackPlayer(int ClientID);

    void OnPlayerAction(int ClientID);
    void OnPlayerFreeze(int ClientID);
    void OnPlayerUnfreeze(int ClientID);
    void OnPlayerImpacted(int ClientID, int InitiatorID);
    void OnPlayerKill(int ClientID);
};

#endif
