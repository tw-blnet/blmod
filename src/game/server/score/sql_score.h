/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
/* CSqlScore Class by Sushi Tee*/
#ifndef GAME_SERVER_SCORE_SQL_H
#define GAME_SERVER_SCORE_SQL_H

#include <string>
#include <vector>

#include <engine/map.h>
#include <engine/server/sql_string_helpers.h>
#include <game/prng.h>

#include "../score.h"

class CSqlServer;

// holding relevant data for one thread, and function pointer for return values
template < typename TResult >
struct CSqlData
{
	CSqlData(std::shared_ptr<TResult> pSqlResult) :
		m_pResult(pSqlResult)
	{ }
	std::shared_ptr<TResult> m_pResult;
	virtual ~CSqlData() = default;
};

struct CSqlInitData : CSqlData<CScoreInitResult>
{
	using CSqlData<CScoreInitResult>::CSqlData;
	// current map
	sqlstr::CSqlString<MAX_MAP_LENGTH> m_Map;
};

struct CSqlPlayerRequest : CSqlData<CScorePlayerResult>
{
	using CSqlData<CScorePlayerResult>::CSqlData;
	// object being requested, either map (128 bytes) or player (16 bytes)
	sqlstr::CSqlString<MAX_MAP_LENGTH> m_Name;
	// current map
	sqlstr::CSqlString<MAX_MAP_LENGTH> m_Map;
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_RequestingPlayer;
	// relevant for /top5 kind of requests
	int m_Offset;
};

struct CSqlRandomMapRequest : CSqlData<CScoreRandomMapResult>
{
	using CSqlData<CScoreRandomMapResult>::CSqlData;
	sqlstr::CSqlString<32> m_ServerType;
	sqlstr::CSqlString<MAX_MAP_LENGTH> m_CurrentMap;
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_RequestingPlayer;
	int m_Stars;
};

struct CSqlScoreData : CSqlData<CScorePlayerResult>
{
	using CSqlData<CScorePlayerResult>::CSqlData;

	sqlstr::CSqlString<MAX_MAP_LENGTH> m_Map;
	char m_GameUuid[UUID_MAXSTRSIZE];
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_Name;

	int m_ClientID;
	float m_Time;
	char m_aTimestamp[TIMESTAMP_STR_LENGTH];
	float m_aCpCurrent[NUM_CHECKPOINTS];
	int m_Num;
	bool m_Search;
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
};

struct CSqlTeamScoreData : CSqlData<void>
{
	using CSqlData<void>::CSqlData;
	char m_GameUuid[UUID_MAXSTRSIZE];
	sqlstr::CSqlString<MAX_MAP_LENGTH> m_Map;
	float m_Time;
	char m_aTimestamp[TIMESTAMP_STR_LENGTH];
	unsigned int m_Size;
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_aNames[MAX_CLIENTS];
};

struct CSqlTeamSave : CSqlData<CScoreSaveResult>
{
	using CSqlData<CScoreSaveResult>::CSqlData;
	virtual ~CSqlTeamSave() {};

	char m_ClientName[MAX_NAME_LENGTH];

	char m_Map[MAX_MAP_LENGTH];
	char m_Code[128];
	char m_aGeneratedCode[128];
	char m_Server[5];
};

struct CSqlTeamLoad : CSqlData<CScoreSaveResult>
{
	using CSqlData<CScoreSaveResult>::CSqlData;
	sqlstr::CSqlString<128> m_Code;
	sqlstr::CSqlString<MAX_MAP_LENGTH> m_Map;
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_RequestingPlayer;
	int m_ClientID;
	// struct holding all player names in the team or an empty string
	char m_aClientNames[MAX_CLIENTS][MAX_NAME_LENGTH];
	int m_aClientID[MAX_CLIENTS];
	int m_NumPlayer;
};

struct CSqlLoginData : CSqlData<CScoreAuthResult>
{
	using CSqlData<CScoreAuthResult>::CSqlData;
	sqlstr::CSqlString<MAX_USERNAME_LENGTH> m_Username;
	sqlstr::CSqlString<MAX_PASSWORD_LENGTH> m_Password;
	unsigned int m_IP;
};

struct CSqlUserData : CSqlData<CScoreAuthResult>
{
	using CSqlData<CScoreAuthResult>::CSqlData;
	unsigned int m_UserID;
};

struct CSqlExperienceData : CSqlData<CScoreExperienceResult>
{
	using CSqlData<CScoreExperienceResult>::CSqlData;
	int m_UserID;
	int m_Count;
};

struct CSqlStatsData : CSqlData<CScoreStatsResult>
{
	using CSqlData<CScoreStatsResult>::CSqlData;
	int m_UserID;
};

struct CSqlRegisterStatsData : CSqlData<void>
{
	using CSqlData<void>::CSqlData;
	int m_UserID;
	int m_Action;
};

// controls one thread
template < typename TResult >
struct CSqlExecData
{
	CSqlExecData(
			bool (*pFuncPtr) (CSqlServer*, const CSqlData<TResult> *, bool),
			CSqlData<TResult> *pSqlResult,
			bool ReadOnly = true
	);
	~CSqlExecData();

	bool (*m_pFuncPtr) (CSqlServer*, const CSqlData<TResult>  *, bool);
	CSqlData<TResult> *m_pSqlData;
	bool m_ReadOnly;

	static void ExecSqlFunc(void *pUser);
};

class IServer;
class CGameContext;

class CSqlScore: public IScore
{
	static LOCK ms_FailureFileLock;

	static bool Init(CSqlServer* pSqlServer, const CSqlData<CScoreInitResult> *pGameData, bool HandleFailure);

	static bool RandomMapThread(CSqlServer* pSqlServer, const CSqlData<CScoreRandomMapResult> *pGameData, bool HandleFailure = false);
	static bool RandomUnfinishedMapThread(CSqlServer* pSqlServer, const CSqlData<CScoreRandomMapResult> *pGameData, bool HandleFailure = false);
	static bool MapVoteThread(CSqlServer* pSqlServer, const CSqlData<CScorePlayerResult> *pGameData, bool HandleFailure = false);

	static bool LoadPlayerDataThread(CSqlServer* pSqlServer, const CSqlData<CScorePlayerResult> *pGameData, bool HandleFailure = false);
	static bool MapInfoThread(CSqlServer* pSqlServer, const CSqlData<CScorePlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowRankThread(CSqlServer* pSqlServer, const CSqlData<CScorePlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowTeamRankThread(CSqlServer* pSqlServer, const CSqlData<CScorePlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowTop5Thread(CSqlServer* pSqlServer, const CSqlData<CScorePlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowTeamTop5Thread(CSqlServer* pSqlServer, const CSqlData<CScorePlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowTimesThread(CSqlServer* pSqlServer, const CSqlData<CScorePlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowPointsThread(CSqlServer* pSqlServer, const CSqlData<CScorePlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowTopPointsThread(CSqlServer* pSqlServer, const CSqlData<CScorePlayerResult> *pGameData, bool HandleFailure = false);
	static bool GetSavesThread(CSqlServer* pSqlServer, const CSqlData<CScorePlayerResult> *pGameData, bool HandleFailure = false);

	static bool SaveTeamThread(CSqlServer* pSqlServer, const CSqlData<CScoreSaveResult> *pGameData, bool HandleFailure = false);
	static bool LoadTeamThread(CSqlServer* pSqlServer, const CSqlData<CScoreSaveResult> *pGameData, bool HandleFailure = false);

	static bool SaveScoreThread(CSqlServer* pSqlServer, const CSqlData<CScorePlayerResult> *pGameData, bool HandleFailure = false);
	static bool SaveTeamScoreThread(CSqlServer* pSqlServer, const CSqlData<void> *pGameData, bool HandleFailure = false);

	static bool RegisterThread(CSqlServer* pSqlServer, const CSqlData<CScoreAuthResult> *pGameData, bool HandleFailure = false);
	static bool LoginThread(CSqlServer* pSqlServer, const CSqlData<CScoreAuthResult> *pGameData, bool HandleFailure = false);
	static bool ChangePasswordThread(CSqlServer* pSqlServer, const CSqlData<CScoreAuthResult> *pGameData, bool HandleFailure = false);
	static bool LinkDiscordThread(CSqlServer* pSqlServer, const CSqlData<CScoreAuthResult> *pGameData, bool HandleFailure = false);

	static bool GiveExperienceThread(CSqlServer* pSqlServer, const CSqlData<CScoreExperienceResult> *pGameData, bool HandleFailure = false);

	static bool LoadStatsThread(CSqlServer* pSqlServer, const CSqlData<CScoreStatsResult> *pGameData, bool HandleFailure = false);
	static bool RegisterStatsThread(CSqlServer* pSqlServer, const CSqlData<void> *pGameData, bool HandleFailure = false);

	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }

	CGameContext *m_pGameServer;
	IServer *m_pServer;

	std::vector<std::string> m_aWordlist;
	CPrng m_Prng;
	void GeneratePassphrase(char *pBuf, int BufSize);

	// returns new SqlResult bound to the player, if no current Thread is active for this player
	std::shared_ptr<CScorePlayerResult> NewSqlPlayerResult(int ClientID);
	// Creates for player database requests
	void ExecPlayerThread(
			bool (*pFuncPtr) (CSqlServer*, const CSqlData<CScorePlayerResult> *, bool),
			const char* pThreadName,
			int ClientID,
			const char* pName,
			int Offset
	);

	// returns true if the player should be rate limited
	bool RateLimitPlayer(int ClientID);

public:
	// keeps track of score-threads
	static std::atomic_int ms_InstanceCount;

	CSqlScore(CGameContext *pGameServer);
	~CSqlScore() {}

	// Requested by game context, shouldn't fail in case the player started another thread
	virtual void RandomMap(int ClientID, int Stars);
	virtual void RandomUnfinishedMap(int ClientID, int Stars);
	virtual void MapVote(int ClientID, const char* MapName);

	virtual void LoadPlayerData(int ClientID);
	// Requested by players (fails if another request by this player is active)
	virtual void MapInfo(int ClientID, const char* MapName);
	virtual void ShowRank(int ClientID, const char* pName);
	virtual void ShowTeamRank(int ClientID, const char* pName);
	virtual void ShowPoints(int ClientID, const char* pName);
	virtual void ShowTimes(int ClientID, const char* pName, int Offset = 1);
	virtual void ShowTimes(int ClientID, int Offset = 1);
	virtual void ShowTop5(int ClientID, int Offset = 1);
	virtual void ShowTeamTop5(int ClientID, int Offset = 1);
	virtual void ShowTopPoints(int ClientID, int Offset = 1);
	virtual void GetSaves(int ClientID);

	// requested by teams
	virtual void SaveTeam(int ClientID, const char* Code, const char* Server);
	virtual void LoadTeam(const char* Code, int ClientID);

	// Game relevant not allowed to fail due to an ongoing SQL request.
	virtual void SaveScore(int ClientID, float Time, const char *pTimestamp,
			float CpTime[NUM_CHECKPOINTS], bool NotEligible);
	virtual void SaveTeamScore(int* aClientIDs, unsigned int Size, float Time, const char *pTimestamp);

	// Accounts related
	virtual void Register(int ClientID, const char* Username, const char* Password);
	virtual void Login(int ClientID, const char* Username, const char* Password);
	virtual void Logout(int ClientID);
	virtual void ChangePassword(int ClientID, const char* Password);
	virtual void LinkDiscord(int ClientID);

	virtual void GiveExperience(int ClientID, int Count);

	virtual void LoadStats(int ClientID);
	virtual void RegisterStats(int ClientID, int Action);

	virtual void OnShutdown();
};

#endif // GAME_SERVER_SCORE_SQL_H
