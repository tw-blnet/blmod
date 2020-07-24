#include "arenas.h"
#include <game/server/gamecontext.h>
#include <game/server/save.h>
#include <game/server/gamemodes/DDRace.h>

CArenasManager::CArenasManager(class CGameContext *pGameServer) : m_pGameContext(pGameServer) {}

void CArenasManager::Tick()
{
	if (m_pGameContext->Server()->Tick() % 5 != 0)
		return;

	for (auto & fight : m_aFights)
	{
		if (fight.second.m_MatchStartTick >= 0)
			continue;

		int CreatorID = -1;
		for (auto & participant : fight.second.m_aParticipants)
		{
			if (participant.m_Status == PARTICIPANT_OWNER)
				CreatorID = participant.m_ClientID;
		}

		for (auto & participant : fight.second.m_aParticipants)
		{
			if (participant.m_Status != PARTICIPANT_INVITED)
				continue;

			if (m_pGameContext->Server()->Tick() - participant.m_InviteTick < m_pGameContext->Server()->TickSpeed() * 15)
				continue;

			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "[Arena] Your invite to a fight by %s has expired", m_pGameContext->Server()->ClientName(CreatorID));
			m_pGameContext->SendChatTarget(participant.m_ClientID, aBuf);

			str_format(aBuf, sizeof(aBuf), "[Arena] %s ignored your invite", m_pGameContext->Server()->ClientName(participant.m_ClientID));
			m_pGameContext->SendChatTarget(CreatorID, aBuf);

			DeclineFight(CreatorID, participant.m_ClientID, true);
			return;
		}
	}
}

int CArenasManager::AddArena(const char *pName, int Tele)
{
	int id = -1;
	while (id < 0 || m_aArenas.count(id))
		id = frandom() * 99;

	CArena *pArena = &m_aArenas[id];
	pArena->m_Tele = Tele;
	str_copy(pArena->m_Name, pName, sizeof(pArena->m_Name));

	return id;
}

int CArenasManager::ArenasCount()
{
	return m_aArenas.size();
}

int CArenasManager::FindArena(const char *pName)
{
	for (auto & pair : m_aArenas)
		if (str_comp_nocase(pName, pair.second.m_Name) == 0)
			return pair.first;
	return -1;
}

const char* CArenasManager::GetArenaName(int ArenaID)
{
	if (m_aArenas.count(ArenaID) == 0)
		return 0;

	return m_aArenas[ArenaID].m_Name;
}

int CArenasManager::GetArenaByIndex(unsigned int Index)
{
	if (Index >= m_aArenas.size())
		return -1;

	auto iter = m_aArenas.begin();
	std::advance(iter, Index);

	return iter->first;
}

void CArenasManager::RemoveArena(int ArenaID)
{
	auto iter = m_aArenas.find(ArenaID);
	if (iter == m_aArenas.end())
		return;

	m_aArenas.erase(iter);
}

int CArenasManager::NewFight(std::vector<int> Participants, int ScoreLimit, int Shotgun, int Grenade, int Laser, int Endless)
{
	if (Participants.size() < 2)
		return -1;

	int Creator = -1;

	for (int Target : Participants)
	{
		if (Creator == Target)
		{
			m_pGameContext->SendChatTarget(Creator, "[Arena] You can not fight yourself");
			return -1;
		}

		if (Creator < 0)
			Creator = Target;

		if (IsClientInFight(Target))
		{
			if (Target == Creator)
				m_pGameContext->SendChatTarget(Creator, "[Arena] You can not start a fight right now");
			else
				m_pGameContext->SendChatTarget(Creator, "[Arena] The target player is already in a fight");
			return -1;
		}

		if (!m_pGameContext->IsClientPlayer(Target))
		{
			m_pGameContext->SendChatTarget(Creator, "[Arena] One of the participants is not in the game");
			return -1;
		}
	}

	if (m_aArenas.size() == 0)
		return -1;

	int id = -1;
	while (id < 0 || m_aFights.count(id))
		id = frandom() * 9999;

	CFight *pFight = &m_aFights[id];
	pFight->m_ArenaID = m_aArenas.begin()->first;
	pFight->m_ScoreLimit = ScoreLimit;
	pFight->m_Shotgun = Shotgun;
	pFight->m_Grenade = Grenade;
	pFight->m_Laser = Laser;
	pFight->m_Endless = Endless;

	pFight->m_RoundsPlayed = 0;
	pFight->m_MatchStartTick = -1;
	pFight->m_Team = -1;


	m_pGameContext->SendChatTarget(Creator, "[Arena] You have sent invite to player");

	for (int ClientID : Participants)
	{
		CParticipant Participant;
		Participant.m_ClientID = ClientID;
		Participant.m_Status = ClientID == Creator ? PARTICIPANT_OWNER : PARTICIPANT_INVITED;
		Participant.m_Score = 0;
		Participant.m_InviteTick = m_pGameContext->Server()->Tick();
		Participant.m_pSaveTee = 0;

		pFight->m_aParticipants.push_back(Participant);

		if (Participant.m_Status != PARTICIPANT_OWNER)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "[Arena] You have been invited to a fight by %s!", m_pGameContext->Server()->ClientName(Creator));
			m_pGameContext->SendChatTarget(ClientID, aBuf);

			str_format(aBuf, sizeof(aBuf), "[Arena] Use `/accept %s` or `/decline %s`", m_pGameContext->Server()->ClientName(Creator), m_pGameContext->Server()->ClientName(Creator));
			m_pGameContext->SendChatTarget(ClientID, aBuf);
		}
	}

	return id;
}

void CArenasManager::EndFight(int Fight)
{
	if (m_aFights.count(Fight) == 0)
		return;

	CFight* pFight = &m_aFights[Fight];

	std::vector<int> Clients;
	for (auto & participant : pFight->m_aParticipants)
		Clients.push_back(participant.m_ClientID);

	bool Started = m_aFights[Fight].m_MatchStartTick >= 0;

	if (Started)
	{
		pFight->m_MatchStartTick = -1; // deactivate death handler

		for (auto & participant : m_aFights[Fight].m_aParticipants)
		{
			CCharacter *pChr = m_pGameContext->GetPlayerChar(participant.m_ClientID);
			if (!pChr)
				continue;

			if (participant.m_pSaveTee)
			{
				participant.m_pSaveTee->load(pChr, 0);
				delete participant.m_pSaveTee;
			}
			else
			{
				pChr->Die(participant.m_ClientID, WEAPON_WORLD);
			}
		}
	}

	m_aFights.erase(m_aFights.find(Fight));
}

bool CArenasManager::IsInvited(int Creator, int ClientID)
{
	for (auto & pair : m_aFights)
	{
		bool Created = false;
		bool Invited = false;

		for (auto & participant : pair.second.m_aParticipants)
		{
			if (participant.m_ClientID == Creator && participant.m_Status == PARTICIPANT_OWNER)
				Created = true;

			if (participant.m_ClientID == ClientID && participant.m_Status == PARTICIPANT_INVITED)
				Invited = true;
		}

		if (Created && Invited)
			return true;
	}

	return false;
}

bool CArenasManager::EligibleForResponse(int Creator, int Target)
{
	if (!m_pGameContext->IsClientPlayer(Creator) || !m_pGameContext->IsClientPlayer(Target))
	{
		m_pGameContext->SendChatTarget(Target, "[Arena] One of the participants is not in the game");
		return false;
	}

	if (IsClientInFight(Target))
	{
		m_pGameContext->SendChatTarget(Target, "[Arena] You are already in a fight");
		return false;
	}

	if (!IsInvited(Creator, Target))
	{
		m_pGameContext->SendChatTarget(Target, "[Arena] You were not invited by the player");
		return false;
	}

	return true;
}

bool CArenasManager::AcceptFight(int Creator, int ClientID)
{
	if (!EligibleForResponse(Creator, ClientID))
		return false;

	int FightID = GetClientFight(Creator, PARTICIPANT_OWNER);
	if (FightID < 0)
		return false;

	CFight* pFight = &m_aFights[FightID];

	bool Found = false;
	for (auto & participant : pFight->m_aParticipants)
		if (participant.m_ClientID == ClientID && participant.m_Status == PARTICIPANT_INVITED)
		{
			participant.m_Status = PARTICIPANT_ACCEPTED;
			Found = true;
			break;
		}

	if (!Found)
		return false;

	TryStart(FightID);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "[Arena] You have accepted the invite by %s", m_pGameContext->Server()->ClientName(Creator));
	m_pGameContext->SendChatTarget(ClientID, aBuf);

	str_format(aBuf, sizeof(aBuf), "[Arena] Player %s has accepted your invite", m_pGameContext->Server()->ClientName(ClientID));
	m_pGameContext->SendChatTarget(Creator, aBuf);

	return true;
}

void CArenasManager::DeclineFight(int Creator, int ClientID, bool Force)
{
	if (!Force && !EligibleForResponse(Creator, ClientID))
		return;

	int FightID = GetClientFight(Creator, PARTICIPANT_OWNER);
	if (FightID < 0)
		return;

	CFight* pFight = &m_aFights[FightID];

	bool Found = false;
	for (auto & participant : pFight->m_aParticipants)
		if (participant.m_ClientID == ClientID && participant.m_Status == PARTICIPANT_INVITED)
		{
			participant.m_Status = PARTICIPANT_DECLINED;
			Found = true;
			break;
		}

	if (!Found)
		return;

	if (!Force)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "[Arena] You have declined the invite by %s", m_pGameContext->Server()->ClientName(Creator));
		m_pGameContext->SendChatTarget(ClientID, aBuf);

		str_format(aBuf, sizeof(aBuf), "[Arena] Player %s has declined your invite", m_pGameContext->Server()->ClientName(ClientID));
		m_pGameContext->SendChatTarget(Creator, aBuf);
	}

	TryStart(FightID);
}

bool CArenasManager::TryStart(int Fight)
{
	CFight* pFight = &m_aFights[Fight];

	if (pFight->m_MatchStartTick >= 0)
		return false;

	bool AllJoin = true;
	for (auto & participant : pFight->m_aParticipants)
	{
		if (participant.m_Status == PARTICIPANT_INVITED)
		{
			AllJoin = false;
			break;
		}
	}

	if (!AllJoin)
		return false;

	bool ContinueSearch = true;
	while (ContinueSearch)
	{
		auto iter = std::find_if(pFight->m_aParticipants.begin(), pFight->m_aParticipants.end(), [](CParticipant & participant)
		{
			return participant.m_Status == PARTICIPANT_DECLINED;
		});

		if (iter == pFight->m_aParticipants.end())
		{
			ContinueSearch = false;
			break;
		}

		pFight->m_aParticipants.erase(iter);
	}

	if (pFight->m_aParticipants.size() < 2)
	{
		EndFight(Fight);
		return false;
	}

	CGameControllerDDRace *pGameControllerDDRace = (CGameControllerDDRace *)m_pGameContext->m_pController;

	pFight->m_Team = 1;
	while (pFight->m_Team < TEAM_SUPER && pGameControllerDDRace->m_Teams.Count(pFight->m_Team))
	{
		pFight->m_Team++;
	}

	if (pFight->m_Team >= 64)
	{
		EndFight(Fight);
		return false;
	}

	for (auto & participant : pFight->m_aParticipants)
	{
		CCharacter *pChr = m_pGameContext->GetPlayerChar(participant.m_ClientID);
		if (!pChr)
			continue;

		participant.m_pSaveTee = new CSaveTee;
		participant.m_pSaveTee->save(pChr);
	}

	Respawn(Fight);

	pFight->m_MatchStartTick = m_pGameContext->Server()->Tick();

	return true;
}

int CArenasManager::GetClientFight(int ClientID, int Status)
{
	for (auto & pair : m_aFights)
		for (auto & participant : pair.second.m_aParticipants)
			if (participant.m_ClientID == ClientID)
				if (participant.m_Status != PARTICIPANT_DECLINED && participant.m_Status != PARTICIPANT_INVITED && (Status == -1 || participant.m_Status == Status))
					return pair.first;

	return -1;
}

bool CArenasManager::IsClientInFight(int ClientID)
{
	return GetClientFight(ClientID) >= 0;
}

bool CArenasManager::HandleDeath(int ClientID)
{
	int Fight = GetClientFight(ClientID);
	if (Fight < 0)
		return true;

	CFight *pFight = &m_aFights[Fight];

	if (pFight->m_MatchStartTick < 0)
		return true;

	pFight->m_RoundsPlayed++;

	// works only for 2 players
	int ParticipantIndex = pFight->m_aParticipants[0].m_ClientID == ClientID ? 0 : 1;
	pFight->m_aParticipants[1 - ParticipantIndex].m_Score++;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "[Arena] Score: %s=%d, %s=%d", m_pGameContext->Server()->ClientName(pFight->m_aParticipants[0].m_ClientID), pFight->m_aParticipants[0].m_Score, m_pGameContext->Server()->ClientName(pFight->m_aParticipants[1].m_ClientID), pFight->m_aParticipants[1].m_Score);
	m_pGameContext->SendChatTarget(pFight->m_aParticipants[0].m_ClientID, aBuf);
	m_pGameContext->SendChatTarget(pFight->m_aParticipants[1].m_ClientID, aBuf);

	if (pFight->m_aParticipants[1 - ParticipantIndex].m_Score >= pFight->m_ScoreLimit)
	{
		EndFight(Fight);
		return false;
	}

	Respawn(Fight);

	return false;
}

void CArenasManager::HandleLeft(int ClientID)
{
	bool Continue = false;

	do {
		Continue = false;

		for (auto & FightPair : m_aFights)
		{
			auto & Participants = FightPair.second.m_aParticipants;

			auto iter = std::find_if(Participants.begin(), Participants.end(), [ClientID](CParticipant & Participant)
			{
				return Participant.m_ClientID == ClientID;
			});

			if (iter == Participants.end() || iter->m_Status == PARTICIPANT_DECLINED)
				continue;

			if (iter->m_pSaveTee)
				delete iter->m_pSaveTee;
			Participants.erase(iter);

			if (Participants.size() < 2)
				EndFight(FightPair.first);
			else
				TryStart(FightPair.first);

			Continue = true;
			break;
		}
	} while (Continue);
}

void CArenasManager::Respawn(int Fight)
{
	CFight *pFight = &m_aFights[Fight];

	if (m_aArenas.count(pFight->m_ArenaID) == 0)
	{
		EndFight(Fight);
		return;
	}

	CArena *pArena = &m_aArenas[pFight->m_ArenaID];

	CGameControllerDDRace *pGameControllerDDRace = (CGameControllerDDRace *)m_pGameContext->m_pController;
	const int TeleNum = pArena->m_Tele;
	const int TeleOutsCount = pGameControllerDDRace->m_TeleOuts[TeleNum-1].size();

	int TeleOut = (rand() / (float) RAND_MAX) * TeleOutsCount; // usually teleouts should be placed together by 2

	for (auto & participant : pFight->m_aParticipants)
	{
		CCharacter *pChr = m_pGameContext->GetPlayerChar(participant.m_ClientID);
		if (!pChr)
			continue;

		vec2 Pos = pGameControllerDDRace->m_TeleOuts[TeleNum-1][TeleOut];

		// works only for 2 players
		TeleOut = (TeleOut + ((TeleOut % 2) ? -1 : 1)) % TeleOutsCount;

		pChr->m_Pos = Pos;
		pChr->m_PrevPos = Pos;
		pChr->Core()->m_Pos = Pos;
		pChr->Core()->m_Vel = vec2(0, 0);
		pChr->Core()->m_LastVel = vec2(0, 0);

		pChr->SetSolo(false);

		pChr->m_DeepFreeze = 0;
		pChr->UnFreeze();
		pChr->Freeze(1);

		pChr->m_EndlessHook = false;

		pChr->SetWeaponGot(WEAPON_GUN, true);
		pChr->SetWeaponGot(WEAPON_SHOTGUN, pFight->m_Shotgun);
		pChr->SetWeaponGot(WEAPON_GRENADE, pFight->m_Grenade);
		pChr->SetWeaponGot(WEAPON_LASER, pFight->m_Laser);
		pChr->SetWeaponGot(WEAPON_NINJA, false);
		pChr->SetWeapon(WEAPON_HAMMER);

		pGameControllerDDRace->DropFlag(pChr);

		pGameControllerDDRace->m_Teams.SetForceCharacterTeam(participant.m_ClientID, pFight->m_Team);
	}
}
