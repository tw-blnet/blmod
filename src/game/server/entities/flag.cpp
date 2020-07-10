/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <game/server/gamecontext.h>
#include "flag.h"

CFlag::CFlag(CGameWorld *pGameWorld, int Team)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG), m_Core(Team)
{
	m_Team = Team;
	m_ProximityRadius = m_Core.ms_PhysSize;
	m_pCarryingCharacter = NULL;
	m_GrabTick = 0;

	m_Core.m_pCarryingCharacterCore = 0;

	Reset();
}

void CFlag::Reset()
{
	m_pCarryingCharacter = NULL;
	m_Core.m_AtStand = 1;
	m_Pos = m_StandPos;
	m_GrabTick = 0;
	m_CarrierFreezedTick = 0;

	m_Core.m_Pos = m_Pos;
	m_Core.m_Vel = vec2(0,0);
}

void CFlag::TickDefered()
{
	m_Pos = m_Core.m_Pos;
}

void CFlag::TickPaused()
{
	++m_DropTick;
	if(m_GrabTick)
		++m_GrabTick;
}

void CFlag::Snap(int SnappingClient)
{
	int Team = 0;
	if(m_pCarryingCharacter)
		Team = m_pCarryingCharacter->Team();

	CPlayer *pPlayer = m_pGameWorld->GameServer()->m_apPlayers[SnappingClient];
	if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->Team() != Team && !pPlayer->IsPaused())
		return;

	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Flag *pFlag = (CNetObj_Flag *)Server()->SnapNewItem(NETOBJTYPE_FLAG, m_Team, sizeof(CNetObj_Flag));
	if(!pFlag)
		return;

	pFlag->m_X = (int)m_Pos.x;
	pFlag->m_Y = (int)m_Pos.y;
	pFlag->m_Team = m_Team;
}

void CFlag::SetCarryingCharacter(CCharacter *Character)
{
	m_CarrierFreezedTick = 0;
	m_pCarryingCharacter = Character;
	m_Core.m_pCarryingCharacterCore = Character ? Character->Core() : 0;
}

void CFlag::SetAtStand(bool AtStand) {
	m_Core.m_AtStand = AtStand;
}
