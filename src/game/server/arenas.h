#ifndef GAME_SERVER_ARENAS_H
#define GAME_SERVER_ARENAS_H

#include <map>
#include <vector>

class CGameContext;
class CSaveTee;

class CArenasManager
{
	static const int MAX_ARENA_NAME_LENGTH = 32;

	enum
	{
		PARTICIPANT_OWNER,
		PARTICIPANT_INVITED,
		PARTICIPANT_ACCEPTED,
		PARTICIPANT_DECLINED,
	};

	struct CParticipant
	{
		int m_ClientID;
		int m_Status;
		int m_InviteTick;
		int m_Score;
		CSaveTee *m_pSaveTee;
	};

	struct CFight
	{
		std::vector<CParticipant> m_aParticipants;

		int m_MatchStartTick;
		int m_Team;

		int m_RoundsPlayed;

		int m_ArenaID;
		int m_ScoreLimit;
		bool m_Shotgun;
		bool m_Grenade;
		bool m_Laser;
		bool m_Endless;
	};

	struct CArena
	{
		int m_Tele;
		char m_Name[MAX_ARENA_NAME_LENGTH];
	};

	class CGameContext *m_pGameContext;

	std::map<int, CFight> m_aFights;
	std::map<int, CArena> m_aArenas;

	int m_DefaultArena = -1;

public:
	CArenasManager(class CGameContext *pGameServer);

	void Tick();

	int AddArena(const char *pName, int Tele); // return arena id
	bool SetDefaultArena(int ArenaID);
	int GetDefaultArena();
	int ArenasCount();
	int FindArena(const char *pName); // return arena id or -1
	const char* GetArenaName(int ArenaID);
	int GetArenaTeleOut(int ArenaID);
	int GetArenaByIndex(unsigned int Index); // return arena id or -1
	void RemoveArena(int ArenaID);

	int NewFight(std::vector<int> Participants, int Arena, int ScoreLimit, bool Shotgun, bool Grenade, bool Laser, bool Endless);
	void EndFight(int Fight);

	bool IsInvited(int Creator, int ClientID);
	bool EligibleForResponse(int Creator, int ClientID);
	bool AcceptFight(int Creator, int ClientID);
	void DeclineFight(int Creator, int ClientID, bool Force=false);
	bool TryStart(int Fight);

	int GetClientFight(int ClientID, int Status = -1); // return -1 if client not in fight
	bool IsClientInFight(int ClientID);

	bool HandleDeath(int ClientID);
	void HandleLeft(int ClientID);

	void Respawn(int Fight);
};

#endif
