/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef GAME_SERVER_ENTITIES_FLAG_H
#define GAME_SERVER_ENTITIES_FLAG_H

#include <game/server/entity.h>
#include <game/gamecore.h>

class CFlag : public CEntity
{
public:
	vec2 m_StandPos;

	CFlagCore m_Core;

	int m_Team;
	int m_DropTick;
	int m_GrabTick;
	int m_CarrierFreezedTick;

	CFlag(CGameWorld *pGameWorld, int Team);

	virtual void Reset();
	virtual void TickDefered();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

	CCharacter *GetCarryingCharacter() const { return m_pCarryingCharacter; }
	void SetCarryingCharacter(CCharacter *Character);

	bool GetAtStand() const { return m_Core.m_AtStand; }
	void SetAtStand(bool AtStand);

private:
	CCharacter *m_pCarryingCharacter;
};

#endif
