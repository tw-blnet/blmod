#include "blocktracker.h"
#include <game/server/gamecontext.h>

CBlockTracker::CBlockTracker(class CGameContext *pGameServer) : m_pGameContext(pGameServer)
{
    for (int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
        m_aTrackedPlayers[ClientID].m_Tracked = false;
}

float CBlockTracker::SecondsPassed(int SinceTick)
{
    int Tick = m_pGameContext->Server()->Tick();
    int TickSpeed = m_pGameContext->Server()->TickSpeed();
    return (float) (Tick - SinceTick) / TickSpeed;
}

bool CBlockTracker::Blocked(int ClientID, int BlockerID)
{
    if (!m_aTrackedPlayers[ClientID].m_IsResisted) return false;
    if (!m_pGameContext->PlayerExists(ClientID) || !m_pGameContext->PlayerExists(BlockerID)) return false;
    if (m_pGameContext->Server()->IsClientsSameAddr(ClientID, BlockerID)) return false;

    m_pGameContext->Score()->GiveExperience(BlockerID, EXPERIENCE_FOR_BLOCK);

	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = BlockerID;
	Msg.m_Victim = ClientID;
	Msg.m_Weapon = WEAPON_NINJA;
	Msg.m_ModeSpecial = 0;
	m_pGameContext->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

    return true;
}

void CBlockTracker::Tick()
{
    for (int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
    {
        auto & Player = m_aTrackedPlayers[ClientID];

        if (!Player.m_Tracked) continue;

        if (Player.m_FreezedTick >= 0 &&
            SecondsPassed(Player.m_FreezedTick) >= FREEZED_TO_BLOCK_SECONDS &&
            Player.m_ImpactedClientID >= 0)
        {
            if (Blocked(ClientID, Player.m_ImpactedClientID))
            {
                Player.m_IsResisted = false;
                Player.m_ImpactedClientID = -1;
                Player.m_LastImpactedTick = -1;
            }
        }

        if (Player.m_UnfreezedTick >= 0 &&
            SecondsPassed(Player.m_UnfreezedTick) > UNFREEZED_TO_RESET_SECONDS &&
            SecondsPassed(Player.m_LastImpactedTick) > NO_IMPACT_TO_RESET_SECONDS)
        {
            Player.m_ImpactedClientID = -1;
            Player.m_LastImpactedTick = -1;
        }
    }
}

void CBlockTracker::StartTrackPlayer(int ClientID)
{
    m_aTrackedPlayers[ClientID].m_Tracked = true;
    m_aTrackedPlayers[ClientID].m_IsResisted = false;
    m_aTrackedPlayers[ClientID].m_LastActionTick = -1;
    m_aTrackedPlayers[ClientID].m_ImpactedClientID = -1;
    m_aTrackedPlayers[ClientID].m_LastImpactedTick = -1;
    m_aTrackedPlayers[ClientID].m_FreezedTick = -1;
    m_aTrackedPlayers[ClientID].m_UnfreezedTick = m_pGameContext->Server()->Tick();
    m_aTrackedPlayers[ClientID].m_KilledTick = m_pGameContext->Server()->Tick();
}

void CBlockTracker::StopTrackPlayer(int ClientID)
{
    m_aTrackedPlayers[ClientID].m_Tracked = false;
}

void CBlockTracker::OnPlayerAction(int ClientID)
{
    if (!m_aTrackedPlayers[ClientID].m_Tracked) return;
    if (m_aTrackedPlayers[ClientID].m_FreezedTick != -1) return;

    m_aTrackedPlayers[ClientID].m_LastActionTick = m_pGameContext->Server()->Tick();
}

void CBlockTracker::OnPlayerFreeze(int ClientID)
{
    if (!m_aTrackedPlayers[ClientID].m_Tracked) return;
    if (m_aTrackedPlayers[ClientID].m_FreezedTick != -1) return;

    m_aTrackedPlayers[ClientID].m_FreezedTick = m_pGameContext->Server()->Tick();
    m_aTrackedPlayers[ClientID].m_UnfreezedTick = -1;

    if (SecondsPassed(m_aTrackedPlayers[ClientID].m_LastActionTick) < IMPACT_WINDOW_TO_RESIST_SECONDS)
        m_aTrackedPlayers[ClientID].m_IsResisted = true;

}

void CBlockTracker::OnPlayerUnfreeze(int ClientID)
{
    if (!m_aTrackedPlayers[ClientID].m_Tracked) return;

    m_aTrackedPlayers[ClientID].m_FreezedTick = -1;
    m_aTrackedPlayers[ClientID].m_UnfreezedTick = m_pGameContext->Server()->Tick();
}

void CBlockTracker::OnPlayerImpacted(int ClientID, int InitiatorID)
{
    if (ClientID == InitiatorID) return;
    if (!m_aTrackedPlayers[ClientID].m_Tracked) return;
    if (m_aTrackedPlayers[ClientID].m_FreezedTick >= 0) return;
    if (SecondsPassed(m_aTrackedPlayers[ClientID].m_UnfreezedTick) < NO_IMPACT_AFTER_UNFREEZE_SECONDS) return;

    m_aTrackedPlayers[ClientID].m_ImpactedClientID = InitiatorID;
    m_aTrackedPlayers[ClientID].m_LastImpactedTick = m_pGameContext->Server()->Tick();
}

bool CBlockTracker::OnPlayerKill(int ClientID)
{
    if (!m_aTrackedPlayers[ClientID].m_Tracked) return false;

    bool Result = false;
    if (m_aTrackedPlayers[ClientID].m_ImpactedClientID != -1 &&
        m_aTrackedPlayers[ClientID].m_FreezedTick != -1 &&
        SecondsPassed(m_aTrackedPlayers[ClientID].m_KilledTick) > KILL_INTERVAL_SECONDS)
        Result = Blocked(ClientID, m_aTrackedPlayers[ClientID].m_ImpactedClientID);

    m_aTrackedPlayers[ClientID].m_IsResisted = false;
    m_aTrackedPlayers[ClientID].m_LastActionTick = -1;
    m_aTrackedPlayers[ClientID].m_ImpactedClientID = -1;
    m_aTrackedPlayers[ClientID].m_LastImpactedTick = -1;
    m_aTrackedPlayers[ClientID].m_FreezedTick = -1;
    m_aTrackedPlayers[ClientID].m_UnfreezedTick = m_pGameContext->Server()->Tick();
    m_aTrackedPlayers[ClientID].m_KilledTick = m_pGameContext->Server()->Tick();

    return Result;
}
