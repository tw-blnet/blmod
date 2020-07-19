/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_SERVER_H
#define ENGINE_SERVER_SERVER_H

#include <base/hash.h>
#include <base/math.h>

#include <engine/engine.h>
#include <engine/server.h>

#include <engine/map.h>
#include <engine/shared/demo.h>
#include <engine/shared/protocol.h>
#include <engine/shared/snapshot.h>
#include <engine/shared/network.h>
#include <engine/server/register.h>
#include <engine/shared/console.h>
#include <engine/shared/econ.h>
#include <engine/shared/fifo.h>
#include <engine/shared/netban.h>
#include <engine/shared/uuid_manager.h>

#include <base/tl/array.h>

#include <list>
#include <vector>

#include "antibot.h"
#include "authmanager.h"
#include "name_ban.h"

#if defined (CONF_UPNP)
	#include "upnp.h"
#endif

#if defined (CONF_SQL)
	#include "sql_connector.h"
	#include "sql_server.h"
#endif

class CSnapIDPool
{
	enum
	{
		MAX_IDS = 16*1024,
	};

	class CID
	{
	public:
		short m_Next;
		short m_State; // 0 = free, 1 = allocated, 2 = timed
		int m_Timeout;
	};

	CID m_aIDs[MAX_IDS];

	int m_FirstFree;
	int m_FirstTimed;
	int m_LastTimed;
	int m_Usage;
	int m_InUsage;

public:

	CSnapIDPool();

	void Reset();
	void RemoveFirstTimeout();
	int NewID();
	void TimeoutIDs();
	void FreeID(int ID);
};


class CServerBan : public CNetBan
{
	class CServer *m_pServer;

	template<class T> int BanExt(T *pBanPool, const typename T::CDataType *pData, int Seconds, const char *pReason);

public:
	class CServer *Server() const { return m_pServer; }

	void InitServerBan(class IConsole *pConsole, class IStorage *pStorage, class CServer *pServer);

	virtual int BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason);
	virtual int BanRange(const CNetRange *pRange, int Seconds, const char *pReason);

	static void ConBanExt(class IConsole::IResult *pResult, void *pUser);
};


class CServer : public IServer
{
	class IGameServer *m_pGameServer;
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;
	class IEngineAntibot *m_pAntibot;

#if defined(CONF_UPNP)
	CUPnP m_UPnP;
#endif

#if defined(CONF_SQL)
	lock m_GlobalSqlLock;

	CSqlServer *m_apSqlReadServers[MAX_SQLSERVERS];
	CSqlServer *m_apSqlWriteServers[MAX_SQLSERVERS];
#endif

#if defined(CONF_FAMILY_UNIX)
	UNIXSOCKETADDR m_ConnLoggingDestAddr;
	bool m_ConnLoggingSocketCreated;
	UNIXSOCKET m_ConnLoggingSocket;
#endif

public:
	class IGameServer *GameServer() { return m_pGameServer; }
	class IConsole *Console() { return m_pConsole; }
	class IStorage *Storage() { return m_pStorage; }
	class IEngineAntibot *Antibot() { return m_pAntibot; }

	enum
	{
		MAX_RCONCMD_SEND=16,
	};

	class CClient
	{
	public:

		enum
		{
			STATE_EMPTY = 0,
			STATE_PREAUTH,
			STATE_AUTH,
			STATE_CONNECTING,
			STATE_READY,
			STATE_INGAME,

			SNAPRATE_INIT=0,
			SNAPRATE_FULL,
			SNAPRATE_RECOVER,

			DNSBL_STATE_NONE=0,
			DNSBL_STATE_PENDING,
			DNSBL_STATE_BLACKLISTED,
			DNSBL_STATE_WHITELISTED,
		};

		class CInput
		{
		public:
			int m_aData[MAX_INPUT_SIZE];
			int m_GameTick; // the tick that was chosen for the input
		};

		// connection state info
		int m_State;
		bool m_SupportsMapSha256;
		int m_Latency;
		int m_SnapRate;

		float m_Traffic;
		int64 m_TrafficSince;

		int m_LastAckedSnapshot;
		int m_LastInputTick;
		CSnapshotStorage m_Snapshots;

		CInput m_LatestInput;
		CInput m_aInputs[200]; // TODO: handle input better
		int m_CurrentInput;

		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		int m_Country;
		int m_Score;
		int m_Authed;
		int m_AuthKey;
		int m_AuthTries;
		int m_NextMapChunk;
		int m_Flags;
		bool m_ShowIps;

		const IConsole::CCommandInfo *m_pRconCmdToSend;

		void Reset();

		// DDRace

		NETADDR m_Addr;
		bool m_GotDDNetVersionPacket;
		bool m_DDNetVersionSettled;
		int m_DDNetVersion;
		char m_aDDNetVersionStr[64];
		CUuid m_ConnectionID;

		// DNSBL
		int m_DnsblState;
		std::shared_ptr<CHostLookup> m_pDnsblLookup;

		bool m_Sixup;

		int m_MapOption;
		int m_LastMapChangedTick;
	};

	CClient m_aClients[MAX_CLIENTS];
	int IdMap[MAX_CLIENTS * VANILLA_MAX_CLIENTS];

	CSnapshotDelta m_SnapshotDelta;
	CSnapshotBuilder m_SnapshotBuilder;
	CSnapIDPool m_IDPool;
	CNetServer m_NetServer;
	CEcon m_Econ;
#if defined(CONF_FAMILY_UNIX)
	CFifo m_Fifo;
#endif
	CServerBan m_ServerBan;

	IEngineMap *m_pMap;

	int64 m_GameStartTime;
	//int m_CurrentGameTick;
	int m_RunServer;
	int m_MapReload;
	bool m_ReloadedWhenEmpty;
	int m_RconClientID;
	int m_RconAuthLevel;
	int m_PrintCBIndex;

	int64 m_Lastheartbeat;
	//static NETADDR4 master_server;

	enum
	{
		SIX=0,
		SIXUP,
	};

	char m_aCurrentMap[MAX_PATH_LENGTH];
	std::vector<SHA256_DIGEST> m_aCurrentMapSha256;
	std::vector<unsigned> m_aCurrentMapCrc;
	std::vector<unsigned char *> m_apCurrentMapData;
	std::vector<unsigned int> m_aCurrentMapSize;

	CDemoRecorder m_aDemoRecorder[MAX_CLIENTS+1];
	CRegister m_Register;
	CRegister m_RegSixup;
	CAuthManager m_AuthManager;

	int m_RconRestrict;

	int64 m_ServerInfoFirstRequest;
	int m_ServerInfoNumRequests;

	char m_aErrorShutdownReason[128];

	array<CNameBan> m_aNameBans;

	CServer();

	int TrySetClientName(int ClientID, const char *pName);

	virtual void SetClientName(int ClientID, const char *pName);
	virtual void SetClientClan(int ClientID, char const *pClan);
	virtual void SetClientCountry(int ClientID, int Country);
	virtual void SetClientScore(int ClientID, int Score);
	virtual void SetClientFlags(int ClientID, int Flags);

	virtual int GetClientLastMapChangedTick(int ClientID);
	virtual int GetClientMapOption(int ClientID);
	virtual bool SetClientMapOption(int ClientID, int MapOption);

	void Kick(int ClientID, const char *pReason);
	void Ban(int ClientID, int Seconds, const char *pReason);

	void DemoRecorder_HandleAutoStart();
	bool DemoRecorder_IsRecording();

	//int Tick()
	int64 TickStartTime(int Tick);
	//int TickSpeed()

	int Init();

	void SetRconCID(int ClientID);
	int GetAuthedState(int ClientID);
	const char *GetAuthName(int ClientID);
	void GetMapInfo(char *pMapName, int MapNameSize, int *pMapSize, SHA256_DIGEST *pMapSha256, int *pMapCrc);
	int GetClientInfo(int ClientID, CClientInfo *pInfo);
	void SetClientDDNetVersion(int ClientID, int DDNetVersion);
	void GetClientAddr(int ClientID, char *pAddrStr, int Size);
	const char *ClientName(int ClientID);
	const char *ClientClan(int ClientID);
	int ClientCountry(int ClientID);
	bool ClientIngame(int ClientID);
	bool ClientAuthed(int ClientID);
	int MaxClients() const;
	int ClientCount();
	int DistinctClientCount();

	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID);

	void DoSnapshot();

	static int NewClientCallback(int ClientID, void *pUser, bool Sixup);
	static int NewClientNoAuthCallback(int ClientID, void *pUser);
	static int DelClientCallback(int ClientID, const char *pReason, void *pUser);

	static int ClientRejoinCallback(int ClientID, void *pUser);

	void SendRconType(int ClientID, bool UsernameReq);
	void SendCapabilities(int ClientID);
	void SendMap(int ClientID);
	void SendMapData(int ClientID, int Chunk);
	void SendConnectionReady(int ClientID);
	void SendRconLine(int ClientID, const char *pLine);
	static void SendRconLineAuthed(const char *pLine, void *pUser, bool Highlighted = false);

	void SendRconCmdAdd(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void SendRconCmdRem(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void UpdateClientRconCommands();

	void ForceAuth(int ClientID, int AuthLevel);

	void ProcessClientPacket(CNetChunk *pPacket);

	class CCache {
	public:
		class CCacheChunk {
		public:
			CCacheChunk(const void *pData, int Size);
			CCacheChunk(const CCacheChunk &) = delete;

			int m_DataSize;
			unsigned char m_aData[NET_MAX_PAYLOAD];
		};

		std::list<CCacheChunk> m_lCache;

		CCache();
		~CCache();

		void AddChunk(const void *pData, int Size);
		void Clear();
	};
	CCache m_ServerInfoCache[3 * 2];
	CCache m_SixupServerInfoCache[2];
	bool m_ServerInfoNeedsUpdate;

	void ExpireServerInfo();
	void CacheServerInfo(CCache *pCache, int Type, bool SendClients);
	void CacheServerInfoSixup(CCache *pCache, bool SendClients);
	void SendServerInfo(const NETADDR *pAddr, int Token, int Type, bool SendClients);
	void GetServerInfoSixup(CPacker *pPacker, int Token, bool SendClients);
	bool RateLimitServerInfoConnless();
	void SendServerInfoConnless(const NETADDR *pAddr, int Token, int Type);
	void UpdateServerInfo(bool Resend = false);

	void PumpNetwork();

	char *GetMapName();
	int LoadMap(const char *pMapName);

	void SaveDemo(int ClientID, float Time);
	void StartRecord(int ClientID);
	void StopRecord(int ClientID);
	bool IsRecording(int ClientID);

	void InitRegister(CNetServer *pNetServer, IEngineMasterServer *pMasterServer, IConsole *pConsole);
	int Run();

	static void ConTestingCommands(IConsole::IResult *pResult, void *pUser);
	static void ConRescue(IConsole::IResult *pResult, void *pUser);
	static void ConKick(IConsole::IResult *pResult, void *pUser);
	static void ConStatus(IConsole::IResult *pResult, void *pUser);
	static void ConShutdown(IConsole::IResult *pResult, void *pUser);
	static void ConRecord(IConsole::IResult *pResult, void *pUser);
	static void ConStopRecord(IConsole::IResult *pResult, void *pUser);
	static void ConMapReload(IConsole::IResult *pResult, void *pUser);
	static void ConLogout(IConsole::IResult *pResult, void *pUser);
	static void ConShowIps(IConsole::IResult *pResult, void *pUser);

	static void ConAuthAdd(IConsole::IResult *pResult, void *pUser);
	static void ConAuthAddHashed(IConsole::IResult *pResult, void *pUser);
	static void ConAuthUpdate(IConsole::IResult *pResult, void *pUser);
	static void ConAuthUpdateHashed(IConsole::IResult *pResult, void *pUser);
	static void ConAuthRemove(IConsole::IResult *pResult, void *pUser);
	static void ConAuthList(IConsole::IResult *pResult, void *pUser);

	static void ConNameBan(IConsole::IResult *pResult, void *pUser);
	static void ConNameUnban(IConsole::IResult *pResult, void *pUser);
	static void ConNameBans(IConsole::IResult *pResult, void *pUser);

#if defined (CONF_SQL)
	// console commands for sqlmasters
	static void ConAddSqlServer(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpSqlServers(IConsole::IResult *pResult, void *pUserData);
#endif

	static void ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainMaxclientsperipUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainCommandAccessUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainConsoleOutputLevelUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void LogoutClient(int ClientID, const char *pReason);
	void LogoutKey(int Key, const char *pReason);

	void ConchainRconPasswordChangeGeneric(int Level, const char *pCurrent, IConsole::IResult *pResult);
	static void ConchainRconPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainRconModPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainRconHelperPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

#if defined(CONF_FAMILY_UNIX)
	static void ConchainConnLoggingServerChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
#endif

	void RegisterCommands();


	virtual int SnapNewID();
	virtual void SnapFreeID(int ID);
	virtual void *SnapNewItem(int Type, int ID, int Size);
	void SnapSetStaticsize(int ItemType, int Size);

	// DDRace

	void GetClientAddr(int ClientID, NETADDR *pAddr);
	int m_aPrevStates[MAX_CLIENTS];
	const char *GetAnnouncementLine(char const *FileName);
	unsigned m_AnnouncementLastLine;
	void RestrictRconOutput(int ClientID) { m_RconRestrict = ClientID; }

	virtual int* GetIdMap(int ClientID);

	void InitDnsbl(int ClientID);
	bool DnsblWhite(int ClientID)
	{
		return m_aClients[ClientID].m_DnsblState == CClient::DNSBL_STATE_NONE ||
		m_aClients[ClientID].m_DnsblState == CClient::DNSBL_STATE_WHITELISTED;
	}

	void AuthRemoveKey(int KeySlot);
	bool ClientPrevIngame(int ClientID) { return m_aPrevStates[ClientID] == CClient::STATE_INGAME; };
	const char *GetNetErrorString(int ClientID) { return m_NetServer.ErrorString(ClientID); };
	void ResetNetErrorString(int ClientID) { m_NetServer.ResetErrorString(ClientID); };
	bool SetTimedOut(int ClientID, int OrigID);
	void SetTimeoutProtected(int ClientID) { m_NetServer.SetTimeoutProtected(ClientID); };

	void SendMsgRaw(int ClientID, const void *pData, int Size, int Flags);

	bool ErrorShutdown() const { return m_aErrorShutdownReason[0] != 0; }
	void SetErrorShutdown(const char *pReason);

	bool IsSixup(int ClientID) const { return m_aClients[ClientID].m_Sixup; }

#ifdef CONF_FAMILY_UNIX
	enum CONN_LOGGING_CMD
	{
		OPEN_SESSION=1,
		CLOSE_SESSION=2,
	};

	void SendConnLoggingCommand(CONN_LOGGING_CMD cmd, const NETADDR *pAddr);
#endif

};

#endif
