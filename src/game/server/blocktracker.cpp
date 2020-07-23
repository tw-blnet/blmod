#include "blocktracker.h"
#include <game/server/gamecontext.h>

CBlockTracker::CBlockTracker(class CGameContext *pGameServer) : m_pGameContext(pGameServer)
{
    for (int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
        m_aTrackedPlayers[ClientID].m_Tracked = false;
}

void CBlockTracker::Tick()
{

}

void CBlockTracker::StartTrackPlayer(int ClientID)
{

}

void CBlockTracker::StopTrackPlayer(int ClientID)
{

}

void CBlockTracker::OnPlayerAction(int ClientID)
{

}

void CBlockTracker::OnPlayerFreeze(int ClientID)
{

}

void CBlockTracker::OnPlayerUnfreeze(int ClientID)
{

}

void CBlockTracker::OnPlayerImpacted(int ClientID, int InitiatorID)
{

}

void CBlockTracker::OnPlayerKill(int ClientID)
{

}
