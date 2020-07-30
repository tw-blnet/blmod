/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include "player.h"

#include <engine/server.h>
#include <engine/server/server.h>
#include "gamecontext.h"
#include <game/gamecore.h>
#include <game/version.h>
#include <game/server/teams.h>
#include "gamemodes/DDRace.h"
#include <time.h>

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, int Team)
{
	m_pGameServer = pGameServer;
	m_ClientID = ClientID;
	m_Team = GameServer()->m_pController->ClampTeam(Team);
	m_NumInputs = 0;
	Reset();
	GameServer()->Antibot()->OnPlayerInit(m_ClientID);
	GameServer()->m_pController->m_BlockTracker.StartTrackPlayer(m_ClientID);
}

CPlayer::~CPlayer()
{
	GameServer()->m_pController->m_BlockTracker.StopTrackPlayer(m_ClientID);
	GameServer()->Antibot()->OnPlayerDestroy(m_ClientID);
	delete m_pLastTarget;
	delete m_pCharacter;
	m_pCharacter = 0;
}

void CPlayer::Reset()
{
	m_DieTick = Server()->Tick();
	m_PreviousDieTick = m_DieTick;
	m_JoinTick = Server()->Tick();
	delete m_pCharacter;
	m_pCharacter = 0;
	m_SpectatorID = SPEC_FREEVIEW;
	m_LastActionTick = Server()->Tick();
	m_TeamChangeTick = Server()->Tick();
	m_LastInvited = 0;
	m_WeakHookSpawn = false;

	int *pIdMap = Server()->GetIdMap(m_ClientID);
	for (int i = 1;i < VANILLA_MAX_CLIENTS;i++)
	{
		pIdMap[i] = -1;
	}
	pIdMap[0] = m_ClientID;

	// DDRace

	m_LastCommandPos = 0;
	m_LastPlaytime = 0;
	m_Sent1stAfkWarning = 0;
	m_Sent2ndAfkWarning = 0;
	m_ChatScore = 0;
	m_Moderating = false;
	m_EyeEmote = true;
	if(Server()->IsSixup(m_ClientID))
		m_TimerType = TIMERTYPE_NONE;
	else
		m_TimerType = (g_Config.m_SvDefaultTimerType == TIMERTYPE_GAMETIMER || g_Config.m_SvDefaultTimerType == TIMERTYPE_GAMETIMER_AND_BROADCAST) ? TIMERTYPE_BROADCAST : g_Config.m_SvDefaultTimerType;

	m_DefEmote = EMOTE_NORMAL;
	m_Afk = true;
	m_LastWhisperTo = -1;
	m_LastSetSpectatorMode = 0;
	m_TimeoutCode[0] = '\0';
	delete m_pLastTarget;
	m_pLastTarget = nullptr;
	m_TuneZone = 0;
	m_TuneZoneOld = m_TuneZone;
	m_Halloween = false;
	m_FirstPacket = true;

	m_SendVoteIndex = -1;

	if(g_Config.m_Events)
	{
		time_t rawtime;
		struct tm* timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		if ((timeinfo->tm_mon == 11 && timeinfo->tm_mday == 31) || (timeinfo->tm_mon == 0 && timeinfo->tm_mday == 1))
		{ // New Year
			m_DefEmote = EMOTE_HAPPY;
		}
		else if ((timeinfo->tm_mon == 9 && timeinfo->tm_mday == 31) || (timeinfo->tm_mon == 10 && timeinfo->tm_mday == 1))
		{ // Halloween
			m_DefEmote = EMOTE_ANGRY;
			m_Halloween = true;
		}
		else
		{
			m_DefEmote = EMOTE_NORMAL;
		}
	}
	m_DefEmoteReset = -1;

	GameServer()->Score()->PlayerData(m_ClientID)->Reset();

	m_ShowOthers = g_Config.m_SvShowOthersDefault;
	m_ShowAll = g_Config.m_SvShowAllDefault;
	m_ShowDistance = vec2(1000, 800);
	m_SpecTeam = 0;
	m_NinjaJetpack = false;

	m_Paused = PAUSE_NONE;
	m_DND = false;

	m_LastPause = 0;
	m_Score = 0;
	m_HasFinishScore = false;

	// Variable initialized:
	m_Last_Team = 0;
	m_LastSQLQuery = 0;
	m_ScoreQueryResult = nullptr;
	m_ScoreFinishResult = nullptr;
	m_ScoreAuthResult = nullptr;
	m_ScoreExperienceResult = nullptr;
	m_ScoreStatsResult = nullptr;

	int64 Now = Server()->Tick();
	int64 TickSpeed = Server()->TickSpeed();
	// If the player joins within ten seconds of the server becoming
	// non-empty, allow them to vote immediately. This allows players to
	// vote after map changes or when they join an empty server.
	//
	// Otherwise, block voting in the beginning after joining.
	if(Now > GameServer()->m_NonEmptySince + 10 * TickSpeed)
		m_FirstVoteTick = Now + g_Config.m_SvJoinVoteDelay * TickSpeed;
	else
		m_FirstVoteTick = Now;

	m_NotEligibleForFinish = false;
	m_EligibleForFinishCheck = 0;
	m_VotedForPractice = false;

	m_Account.m_Authenticated = false;

	m_Brush.m_Entity = CGameWorld::ENTTYPE_PICKUP;
	m_Brush.m_Rounding = true;
	m_Brush.m_Data.m_Pickup.m_Type = POWERUP_HEALTH;
	m_Brush.m_Data.m_Pickup.m_SubType = 0;
}

static int PlayerFlags_SevenToSix(int Flags)
{
	int Six = 0;
	if(Flags&protocol7::PLAYERFLAG_CHATTING)
		Six |= PLAYERFLAG_CHATTING;
	if(Flags&protocol7::PLAYERFLAG_SCOREBOARD)
		Six |= PLAYERFLAG_SCOREBOARD;

	return Six;
}

static int PlayerFlags_SixToSeven(int Flags)
{
	int Seven = 0;
	if(Flags&PLAYERFLAG_CHATTING)
		Seven |= protocol7::PLAYERFLAG_CHATTING;
	if(Flags&PLAYERFLAG_SCOREBOARD)
		Seven |= protocol7::PLAYERFLAG_SCOREBOARD;

	return Seven;
}

void CPlayer::Tick()
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(m_ScoreQueryResult != nullptr && m_ScoreQueryResult.use_count() == 1)
	{
		ProcessScoreResult(*m_ScoreQueryResult);
		m_ScoreQueryResult = nullptr;
	}
	if(m_ScoreFinishResult != nullptr && m_ScoreFinishResult.use_count() == 1)
	{
		ProcessScoreResult(*m_ScoreFinishResult);
		m_ScoreFinishResult = nullptr;
	}
	if(m_ScoreAuthResult != nullptr && m_ScoreAuthResult.use_count() == 1)
	{
		ProcessAuthResult(*m_ScoreAuthResult);
		m_ScoreAuthResult = nullptr;
	}
	if(m_ScoreExperienceResult != nullptr && m_ScoreExperienceResult.use_count() == 1)
	{
		ProcessExperienceResult(*m_ScoreExperienceResult);
		m_ScoreExperienceResult = nullptr;
	}
	if(m_ScoreStatsResult != nullptr && m_ScoreStatsResult.use_count() == 1)
	{
		ProcessStatsResult(*m_ScoreStatsResult);
		m_ScoreStatsResult = nullptr;
	}

	if(!Server()->ClientIngame(m_ClientID))
		return;

	if (m_ChatScore > 0)
		m_ChatScore--;

	Server()->SetClientScore(m_ClientID, m_Score);

	if (m_Moderating && m_Afk)
	{
		m_Moderating = false;
		GameServer()->SendChatTarget(m_ClientID, "Active moderator mode disabled because you are afk.");

		if (!GameServer()->PlayerModerating())
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Server kick/spec votes are no longer actively moderated.");
	}

	// do latency stuff
	{
		IServer::CClientInfo Info;
		if(Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_Accum += Info.m_Latency;
			m_Latency.m_AccumMax = maximum(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = minimum(m_Latency.m_AccumMin, Info.m_Latency);
		}
		// each second
		if(Server()->Tick()%Server()->TickSpeed() == 0)
		{
			m_Latency.m_Avg = m_Latency.m_Accum/Server()->TickSpeed();
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_Accum = 0;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if(Server()->GetNetErrorString(m_ClientID)[0])
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' would have timed out, but can use timeout protection now", Server()->ClientName(m_ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		Server()->ResetNetErrorString(m_ClientID);
	}

	if(!GameServer()->m_World.m_Paused)
	{
		int EarliestRespawnTick = m_PreviousDieTick+Server()->TickSpeed()*3;
		int RespawnTick = maximum(m_DieTick, EarliestRespawnTick)+2;
		if(!m_pCharacter && RespawnTick <= Server()->Tick())
			m_Spawning = true;

		if(m_pCharacter)
		{
			if(m_pCharacter->IsAlive())
			{
				ProcessPause();
				if(!m_Paused)
					m_ViewPos = m_pCharacter->m_Pos;
			}
			else if(!m_pCharacter->IsPaused())
			{
				delete m_pCharacter;
				m_pCharacter = 0;
			}
		}
		else if(m_Spawning && !m_WeakHookSpawn)
			TryRespawn();
	}
	else
	{
		++m_DieTick;
		++m_PreviousDieTick;
		++m_JoinTick;
		++m_LastActionTick;
		++m_TeamChangeTick;
	}

	m_TuneZoneOld = m_TuneZone; // determine needed tunings with viewpos
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_ViewPos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	if (m_TuneZone != m_TuneZoneOld) // don't send tunings all the time
	{
		GameServer()->SendTuningParams(m_ClientID, m_TuneZone);
	}
}

void CPlayer::PostTick()
{
	// update latency value
	if(m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				m_aActLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
		}
	}

	// update view pos for spectators
	if((m_Team == TEAM_SPECTATORS || m_Paused) && m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID] && GameServer()->m_apPlayers[m_SpectatorID]->GetCharacter())
		m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->GetCharacter()->m_Pos;
}

void CPlayer::PostPostTick()
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(!Server()->ClientIngame(m_ClientID))
		return;

	if(!GameServer()->m_World.m_Paused && !m_pCharacter && m_Spawning && m_WeakHookSpawn)
		TryRespawn();
}

void CPlayer::Snap(int SnappingClient)
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(!Server()->ClientIngame(m_ClientID))
		return;

	int id = m_ClientID;
	if(SnappingClient > -1 && !Server()->Translate(id, SnappingClient))
		return;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, id, sizeof(CNetObj_ClientInfo)));
	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, Server()->ClientName(m_ClientID));
	StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
	StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_SkinName);
	pClientInfo->m_UseCustomColor = m_TeeInfos.m_UseCustomColor;
	pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
	pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;

	int ClientVersion = GetClientVersion();
	int Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	int Score = m_Score;

	// send 0 if times of others are not shown
	if(SnappingClient != m_ClientID && g_Config.m_SvHideScore)
		Score = -9999;

	if(SnappingClient < 0 || !Server()->IsSixup(SnappingClient))
	{
		CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, id, sizeof(CNetObj_PlayerInfo)));
		if(!pPlayerInfo)
			return;

		pPlayerInfo->m_Latency = Latency;
		pPlayerInfo->m_Score = Score;
		pPlayerInfo->m_Local = (int)(m_ClientID == SnappingClient && (m_Paused != PAUSE_PAUSED || ClientVersion >= VERSION_DDNET_OLD));
		pPlayerInfo->m_ClientID = id;
		pPlayerInfo->m_Team = (ClientVersion < VERSION_DDNET_OLD || m_Paused != PAUSE_PAUSED || m_ClientID != SnappingClient) && m_Paused < PAUSE_SPEC ? m_Team : TEAM_SPECTATORS;

		if(m_ClientID == SnappingClient && m_Paused == PAUSE_PAUSED && ClientVersion < VERSION_DDNET_OLD)
			pPlayerInfo->m_Team = TEAM_SPECTATORS;
	}
	else
	{
		protocol7::CNetObj_PlayerInfo *pPlayerInfo = static_cast<protocol7::CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, id, sizeof(protocol7::CNetObj_PlayerInfo)));
		if(!pPlayerInfo)
			return;

		pPlayerInfo->m_PlayerFlags = PlayerFlags_SixToSeven(m_PlayerFlags);
		if(Server()->ClientAuthed(m_ClientID))
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_ADMIN;

		// Times are in milliseconds for 0.7
		pPlayerInfo->m_Score = Score;
		pPlayerInfo->m_Latency = Latency;
	}

	if(m_ClientID == SnappingClient && (m_Team == TEAM_SPECTATORS || m_Paused))
	{
		if(SnappingClient < 0 || !Server()->IsSixup(SnappingClient))
		{
			CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
			if(!pSpectatorInfo)
				return;

			pSpectatorInfo->m_SpectatorID = m_SpectatorID;
			pSpectatorInfo->m_X = m_ViewPos.x;
			pSpectatorInfo->m_Y = m_ViewPos.y;
		}
		else
		{
			protocol7::CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<protocol7::CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(protocol7::CNetObj_SpectatorInfo)));
			if(!pSpectatorInfo)
				return;

			pSpectatorInfo->m_SpecMode = m_SpectatorID == SPEC_FREEVIEW ? protocol7::SPEC_FREEVIEW : protocol7::SPEC_PLAYER;
			pSpectatorInfo->m_SpectatorID = m_SpectatorID;
			pSpectatorInfo->m_X = m_ViewPos.x;
			pSpectatorInfo->m_Y = m_ViewPos.y;
		}
	}

	CNetObj_DDNetPlayer *pDDNetPlayer = static_cast<CNetObj_DDNetPlayer *>(Server()->SnapNewItem(NETOBJTYPE_DDNETPLAYER, id, sizeof(CNetObj_DDNetPlayer)));
	if(!pDDNetPlayer)
		return;

	pDDNetPlayer->m_AuthLevel = Server()->GetAuthedState(id);
	pDDNetPlayer->m_Flags = 0;
	if(m_Afk)
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_AFK;
	if(m_Paused == PAUSE_SPEC)
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_SPEC;
	if(m_Paused == PAUSE_PAUSED)
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_PAUSED;

	if(SnappingClient >= 0 && Server()->IsSixup(SnappingClient) && m_pCharacter && m_pCharacter->m_DDRaceState == DDRACE_STARTED &&
		GameServer()->m_apPlayers[SnappingClient]->m_TimerType == TIMERTYPE_SIXUP)
	{
		protocol7::CNetObj_PlayerInfoRace *pRaceInfo = static_cast<protocol7::CNetObj_PlayerInfoRace *>(Server()->SnapNewItem(-protocol7::NETOBJTYPE_PLAYERINFORACE, id, sizeof(protocol7::CNetObj_PlayerInfoRace)));
		pRaceInfo->m_RaceStartTick = m_pCharacter->m_StartTime;
	}

	bool ShowSpec = m_pCharacter && m_pCharacter->IsPaused();

	if(SnappingClient >= 0)
	{
		CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
		ShowSpec = ShowSpec && (
			GameServer()->GetDDRaceTeam(id) == GameServer()->GetDDRaceTeam(SnappingClient)
			|| pSnapPlayer->m_ShowOthers
			|| (pSnapPlayer->GetTeam() == TEAM_SPECTATORS || pSnapPlayer->IsPaused())
			);
	}

	if(ShowSpec)
	{
		CNetObj_SpecChar *pSpecChar = static_cast<CNetObj_SpecChar *>(Server()->SnapNewItem(NETOBJTYPE_SPECCHAR, id, sizeof(CNetObj_SpecChar)));
		pSpecChar->m_X = m_pCharacter->Core()->m_Pos.x;
		pSpecChar->m_Y = m_pCharacter->Core()->m_Pos.y;
	}
}

void CPlayer::FakeSnap()
{
	if(GetClientVersion() >= VERSION_DDNET_OLD)
		return;

	if(Server()->IsSixup(m_ClientID))
		return;

	int FakeID = VANILLA_MAX_CLIENTS - 1;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, FakeID, sizeof(CNetObj_ClientInfo)));

	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, " ");
	StrToInts(&pClientInfo->m_Clan0, 3, "");
	StrToInts(&pClientInfo->m_Skin0, 6, "default");

	if(m_Paused != PAUSE_PAUSED)
		return;

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, FakeID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = m_Latency.m_Min;
	pPlayerInfo->m_Local = 1;
	pPlayerInfo->m_ClientID = FakeID;
	pPlayerInfo->m_Score = -9999;
	pPlayerInfo->m_Team = TEAM_SPECTATORS;

	CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, FakeID, sizeof(CNetObj_SpectatorInfo)));
	if(!pSpectatorInfo)
		return;

	pSpectatorInfo->m_SpectatorID = m_SpectatorID;
	pSpectatorInfo->m_X = m_ViewPos.x;
	pSpectatorInfo->m_Y = m_ViewPos.y;
}

void CPlayer::OnDisconnect(const char *pReason)
{
	GameServer()->m_pController->m_ArenasManager.HandleLeft(m_ClientID);

	KillCharacter();

	if(Server()->ClientIngame(m_ClientID))
	{
		char aBuf[512];
		if(!Server()->IsClientChangeMapOption(m_ClientID))
		{
			if(pReason && *pReason)
				str_format(aBuf, sizeof(aBuf), "'%s' has left the game (%s)", Server()->ClientName(m_ClientID), pReason);
			else
				str_format(aBuf, sizeof(aBuf), "'%s' has left the game", Server()->ClientName(m_ClientID));
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, CGameContext::CHAT_SIX);
		}

		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", m_ClientID, Server()->ClientName(m_ClientID));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);

		bool WasModerator = m_Moderating;

		// Set this to false, otherwise PlayerModerating() will return true.
		m_Moderating = false;

		if (!GameServer()->PlayerModerating() && WasModerator)
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Server kick/spec votes are no longer actively moderated.");
	}

	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	if(g_Config.m_SvTeam != 3)
		Controller->m_Teams.SetForceCharacterTeam(m_ClientID, TEAM_FLOCK);
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput)
{
	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	AfkVoteTimer(NewInput);

	m_NumInputs++;

	if(m_pCharacter && !m_Paused)
		m_pCharacter->OnPredictedInput(NewInput);

	// Magic number when we can hope that client has successfully identified itself
	if(m_NumInputs == 20 && g_Config.m_SvClientSuggestion[0] != '\0' && GetClientVersion() <= VERSION_DDNET_OLD)
		GameServer()->SendBroadcast(g_Config.m_SvClientSuggestion, m_ClientID);
	else if(m_NumInputs == 200 && Server()->IsSixup(m_ClientID))
		GameServer()->SendBroadcast(g_Config.m_SvTranslationNotice, m_ClientID);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
	if(Server()->IsSixup(m_ClientID))
		NewInput->m_PlayerFlags = PlayerFlags_SevenToSix(NewInput->m_PlayerFlags);

	if(NewInput->m_PlayerFlags)
		Server()->SetClientFlags(m_ClientID, NewInput->m_PlayerFlags);

	if(AfkTimer(NewInput->m_TargetX, NewInput->m_TargetY))
		return; // we must return if kicked, as player struct is already deleted
	AfkVoteTimer(NewInput);

	m_TargetPos = vec2(NewInput->m_TargetX, NewInput->m_TargetY);

	if(((!m_pCharacter && m_Team == TEAM_SPECTATORS) || m_Paused) && m_SpectatorID == SPEC_FREEVIEW)
		m_ViewPos = vec2(NewInput->m_TargetX, NewInput->m_TargetY);

	if(NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
	// skip the input if chat is active
		if(m_PlayerFlags&PLAYERFLAG_CHATTING)
			return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = NewInput->m_PlayerFlags;
		return;
	}

	m_PlayerFlags = NewInput->m_PlayerFlags;

	if(m_pCharacter && m_Paused)
		m_pCharacter->ResetInput();

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire&1))
		m_Spawning = true;

	// check for activity
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire&1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
		m_LastActionTick = Server()->Tick();
	}
}

void CPlayer::OnPredictedEarlyInput(CNetObj_PlayerInput *NewInput)
{
	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	if(m_pCharacter && !m_Paused)
		m_pCharacter->OnDirectInput(NewInput);
}

int CPlayer::GetClientVersion() const
{
	return m_pGameServer->GetClientVersion(m_ClientID);
}

CCharacter *CPlayer::GetCharacter()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return 0;
}

void CPlayer::KillCharacter(int Weapon)
{
	bool CanDie = GameServer()->m_pController->m_ArenasManager.HandleDeath(m_ClientID);
	if(!CanDie)
		return;

	if(m_pCharacter)
	{
		m_pCharacter->Die(m_ClientID, Weapon);

		delete m_pCharacter;
		m_pCharacter = 0;
	}
}

void CPlayer::Respawn(bool WeakHook)
{
	if(m_Team != TEAM_SPECTATORS)
	{
		m_WeakHookSpawn = WeakHook;
		m_Spawning = true;
	}
}

CCharacter* CPlayer::ForceSpawn(vec2 Pos)
{
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, Pos);
	m_Team = 0;
	return m_pCharacter;
}

void CPlayer::SetTeam(int Team, bool DoChatMsg)
{
	Team = GameServer()->m_pController->ClampTeam(Team);
	if(m_Team == Team)
		return;

	if(GameServer()->m_pController->m_ArenasManager.IsClientInFight(m_ClientID))
	{
		GameServer()->SendChatTarget(m_ClientID, "You cannot become spectator during fight");
		return;
	}

	char aBuf[512];
	DoChatMsg = false;
	if(DoChatMsg)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(m_ClientID), GameServer()->m_pController->GetTeamName(Team));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

	if(Team == TEAM_SPECTATORS)
	{
		CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
		if(g_Config.m_SvTeam != 3)
			Controller->m_Teams.SetForceCharacterTeam(m_ClientID, TEAM_FLOCK);
	}

	KillCharacter();

	m_Team = Team;
	m_LastSetTeam = Server()->Tick();
	m_LastActionTick = Server()->Tick();
	m_SpectatorID = SPEC_FREEVIEW;
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", m_ClientID, Server()->ClientName(m_ClientID), m_Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	//GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[m_ClientID]);

	protocol7::CNetMsg_Sv_Team Msg;
	Msg.m_ClientID = m_ClientID;
	Msg.m_Team = m_Team;
	Msg.m_Silent = !DoChatMsg;
	Msg.m_CooldownTick = m_LastSetTeam + Server()->TickSpeed() * g_Config.m_SvTeamChangeDelay;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, -1);

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
				GameServer()->m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
		}
	}
}

bool CPlayer::SetTimerType(int TimerType)
{
	if(TimerType == TIMERTYPE_DEFAULT)
	{
		if(Server()->IsSixup(m_ClientID))
			m_TimerType = TIMERTYPE_SIXUP;
		else
			SetTimerType(g_Config.m_SvDefaultTimerType);

		return true;
	}

	if(Server()->IsSixup(m_ClientID))
	{
		if(TimerType == TIMERTYPE_SIXUP || TimerType == TIMERTYPE_NONE)
		{
			m_TimerType = TimerType;
			return true;
		}
		else
			return false;
	}

	if(TimerType == TIMERTYPE_GAMETIMER)
	{
		if(GetClientVersion() >= VERSION_DDNET_GAMETICK)
			m_TimerType = TimerType;
		else
			return false;
	}
	else if(TimerType == TIMERTYPE_GAMETIMER_AND_BROADCAST)
	{
		if(GetClientVersion() >= VERSION_DDNET_GAMETICK)
			m_TimerType = TimerType;
		else
		{
			m_TimerType = TIMERTYPE_BROADCAST;
			return false;
		}
	}
	else
		m_TimerType = TimerType;

	return true;
}

void CPlayer::TryRespawn()
{
	vec2 SpawnPos;

	if(!GameServer()->m_pController->CanSpawn(m_Team, &SpawnPos))
		return;

	m_WeakHookSpawn = false;
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, SpawnPos);
	GameServer()->CreatePlayerSpawn(SpawnPos, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));

	if(g_Config.m_SvTeam == 3)
		m_pCharacter->SetSolo(true);
}

bool CPlayer::AfkTimer(int NewTargetX, int NewTargetY)
{
	/*
		afk timer (x, y = mouse coordinates)
		Since a player has to move the mouse to play, this is a better method than checking
		the player's position in the game world, because it can easily be bypassed by just locking a key.
		Frozen players could be kicked as well, because they can't move.
		It also works for spectators.
		returns true if kicked
	*/

	bool SetWarning = false;
	if(NewTargetX != m_LastTarget_x || NewTargetY != m_LastTarget_y)
	{
		GameServer()->m_pController->m_BlockTracker.OnPlayerAction(m_ClientID);
		m_LastPlaytime = time_get();
		m_LastTarget_x = NewTargetX;
		m_LastTarget_y = NewTargetY;
		SetWarning = true;
	}

	if(Server()->GetAuthedState(m_ClientID))
		return false; // don't kick admins
	if(g_Config.m_SvMaxAfkTime == 0)
		return false; // 0 = disabled

	if(SetWarning)
	{
		m_Sent1stAfkWarning = 0; // afk timer's 1st warning after 50% of sv_max_afk_time
		m_Sent2ndAfkWarning = 0;
	}
	else
	{
		if(!m_Paused)
		{
			// not playing, check how long
			if(m_Sent1stAfkWarning == 0 && m_LastPlaytime < time_get()-time_freq()*(int)(g_Config.m_SvMaxAfkTime*0.5))
			{
				str_format(m_pAfkMsg, sizeof(m_pAfkMsg),
					"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
					(int)(g_Config.m_SvMaxAfkTime*0.5),
					g_Config.m_SvMaxAfkTime
				);
				m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
				m_Sent1stAfkWarning = 1;
			}
			else if(m_Sent2ndAfkWarning == 0 && m_LastPlaytime < time_get()-time_freq()*(int)(g_Config.m_SvMaxAfkTime*0.9))
			{
				str_format(m_pAfkMsg, sizeof(m_pAfkMsg),
					"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
					(int)(g_Config.m_SvMaxAfkTime*0.9),
					g_Config.m_SvMaxAfkTime
				);
				m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
				m_Sent2ndAfkWarning = 1;
			}
			else if(m_LastPlaytime < time_get()-time_freq()*g_Config.m_SvMaxAfkTime)
			{
				m_pGameServer->Server()->Kick(m_ClientID, "Away from keyboard");
				return true;
			}
		}
	}
	return false;
}

void CPlayer::AfkVoteTimer(CNetObj_PlayerInput *NewTarget)
{
	if(g_Config.m_SvMaxAfkVoteTime == 0)
		return;

	if(!m_pLastTarget)
	{
		m_pLastTarget = new CNetObj_PlayerInput(*NewTarget);
		m_LastPlaytime = 0;
		m_Afk = true;
		return;
	}
	else if(mem_comp(NewTarget, m_pLastTarget, sizeof(CNetObj_PlayerInput)) != 0)
	{
		m_LastPlaytime = time_get();
		mem_copy(m_pLastTarget, NewTarget, sizeof(CNetObj_PlayerInput));
	}
	else if(m_LastPlaytime < time_get()-time_freq()*g_Config.m_SvMaxAfkVoteTime)
	{
		m_Afk = true;
		return;
	}

	m_Afk = false;
}

void CPlayer::ProcessPause()
{
	if(m_ForcePauseTime && m_ForcePauseTime < Server()->Tick())
	{
		m_ForcePauseTime = 0;
		Pause(PAUSE_NONE, true);
	}

	if(m_Paused == PAUSE_SPEC && !m_pCharacter->IsPaused() && m_pCharacter->IsGrounded() && m_pCharacter->m_Pos == m_pCharacter->m_PrevPos)
	{
		m_pCharacter->Pause(true);
		GameServer()->CreateDeath(m_pCharacter->m_Pos, m_ClientID, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
		GameServer()->CreateSound(m_pCharacter->m_Pos, SOUND_PLAYER_DIE, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
	}
}

int CPlayer::Pause(int State, bool Force)
{
	if(State < PAUSE_NONE || State > PAUSE_SPEC) // Invalid pause state passed
		return 0;

	if(!m_pCharacter)
		return 0;

	char aBuf[128];
	if(State != m_Paused)
	{
		// Get to wanted state
		switch(State){
		case PAUSE_PAUSED:
		case PAUSE_NONE:
			if(m_pCharacter->IsPaused()) // First condition might be unnecessary
			{
				if(!Force && m_LastPause && m_LastPause + g_Config.m_SvSpecFrequency * Server()->TickSpeed() > Server()->Tick())
				{
					GameServer()->SendChatTarget(m_ClientID, "Can't /spec that quickly.");
					return m_Paused; // Do not update state. Do not collect $200
				}
				m_pCharacter->Pause(false);
				GameServer()->CreatePlayerSpawn(m_pCharacter->m_Pos, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
			}
			// fall-thru
		case PAUSE_SPEC:
			if(g_Config.m_SvPauseMessages)
			{
				str_format(aBuf, sizeof(aBuf), (State > PAUSE_NONE) ? "'%s' speced" : "'%s' resumed", Server()->ClientName(m_ClientID));
				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
			}
			break;
		}

		// Update state
		m_Paused = State;
		m_LastPause = Server()->Tick();
	}

	return m_Paused;
}

int CPlayer::ForcePause(int Time)
{
	m_ForcePauseTime = Server()->Tick() + Server()->TickSpeed() * Time;

	if(g_Config.m_SvPauseMessages)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "'%s' was force-paused for %ds", Server()->ClientName(m_ClientID), Time);
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

	return Pause(PAUSE_SPEC, true);
}

int CPlayer::IsPaused()
{
	return m_ForcePauseTime ? m_ForcePauseTime : -1 * m_Paused;
}

bool CPlayer::IsPlaying()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return true;
	return false;
}

void CPlayer::SpectatePlayerName(const char *pName)
{
	if(!pName)
		return;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i != m_ClientID && Server()->ClientIngame(i) && !str_comp(pName, Server()->ClientName(i)))
		{
			m_SpectatorID = i;
			return;
		}
	}
}

void CPlayer::ShowLevelProgress(int ExperienceIncrement, int ExperienceMultiplier)
{
	char aBuf[256];

	if (m_Account.m_Authenticated)
	{
		int NextLevelExp = GameServer()->Score()->ExperienceRequired(m_Account.m_Level+1);

		if (ExperienceIncrement)
			if (ExperienceMultiplier == 1)
				str_format(aBuf, sizeof(aBuf), "Level: %d\nExp: %d/%d (+%d)", m_Account.m_Level, m_Account.m_Experience, NextLevelExp, ExperienceIncrement);
			else
				str_format(aBuf, sizeof(aBuf), "Level: %d\nExp: %d/%d (+%d×%d)", m_Account.m_Level, m_Account.m_Experience, NextLevelExp, ExperienceIncrement, ExperienceMultiplier);
		else
			str_format(aBuf, sizeof(aBuf), "Level: %d\nExp: %d/%d", m_Account.m_Level, m_Account.m_Experience, NextLevelExp);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "Login to get experience\nUse /register to create account");
	}

	// fill rest of string with spaces to align broadcast to left
	size_t Pos = 0;
	for (Pos = 0; aBuf[Pos] && Pos < sizeof(aBuf); Pos++);
	for (; Pos < sizeof(aBuf)-1; aBuf[Pos]=' ', Pos++);
	aBuf[sizeof(aBuf)-1] = 0;

	GameServer()->SendBroadcast(aBuf, m_ClientID);
}

void CPlayer::ProcessScoreResult(CScorePlayerResult &Result)
{
	if(Result.m_Done) // SQL request was successful
	{
		switch(Result.m_MessageKind)
		{
		case CScorePlayerResult::DIRECT:
			for(int i = 0; i < CScorePlayerResult::MAX_MESSAGES; i++)
			{
				if(Result.m_Data.m_aaMessages[i][0] == 0)
					break;
				GameServer()->SendChatTarget(m_ClientID, Result.m_Data.m_aaMessages[i]);
			}
			break;
		case CScorePlayerResult::ALL:
			for(int i = 0; i < CScorePlayerResult::MAX_MESSAGES; i++)
			{
				if(Result.m_Data.m_aaMessages[i][0] == 0)
					break;
				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, Result.m_Data.m_aaMessages[i], m_ClientID);
			}
			break;
		case CScorePlayerResult::BROADCAST:
			if(Result.m_Data.m_Broadcast[0] != 0)
				GameServer()->SendBroadcast(Result.m_Data.m_Broadcast, -1);
			break;
		case CScorePlayerResult::MAP_VOTE:
			GameServer()->m_VoteKick = false;
			GameServer()->m_VoteSpec = false;
			GameServer()->m_LastMapVote = time_get();

			char aCmd[256];
			str_format(aCmd, sizeof(aCmd),
					"sv_reset_file types/%s/flexreset.cfg; change_map \"%s\"",
					Result.m_Data.m_MapVote.m_Server, Result.m_Data.m_MapVote.m_Map);

			char aChatmsg[512];
			str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s' (%s)",
					Server()->ClientName(m_ClientID), Result.m_Data.m_MapVote.m_Map, "/map");

			GameServer()->CallVote(m_ClientID, Result.m_Data.m_MapVote.m_Map, aCmd, "/map", aChatmsg);
			break;
		case CScorePlayerResult::PLAYER_INFO:
			GameServer()->Score()->PlayerData(m_ClientID)->Set(
					Result.m_Data.m_Info.m_Time,
					Result.m_Data.m_Info.m_CpTime
			);
			// m_Score = Result.m_Data.m_Info.m_Score;
			m_HasFinishScore = Result.m_Data.m_Info.m_HasFinishScore;
			// -9999 stands for no time and isn't displayed in scoreboard, so
			// shift the time by a second if the player actually took 9999
			// seconds to finish the map.
			// if(m_HasFinishScore && m_Score == -9999)
			// 	m_Score = -10000;
			Server()->ExpireServerInfo();
			break;
		}
	}
}

void CPlayer::ProcessAuthResult(CScoreAuthResult &Result)
{
	if(!Result.m_Done)
		return;

	switch(Result.m_Action)
	{
		case CScoreAuthResult::REGISTER:
		{
			if (Result.m_Data.m_Register.m_UserID < 0)
			{
				GameServer()->SendChatTarget(m_ClientID, "Account can't be registered! Maybe your username is already in use?");
				break;
			}

			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Account `%s` (id #%d) is registered! Now you can use /login", Result.m_Data.m_Register.m_Username, Result.m_Data.m_Register.m_UserID);
			GameServer()->SendChatTarget(m_ClientID, aBuf);
			break;
		}
		case CScoreAuthResult::LOGIN:
		{
			if (Result.m_Data.m_Register.m_UserID < 0)
			{
				GameServer()->SendChatTarget(m_ClientID, "Wrong credentials! Check your login and password");
				break;
			}

			m_Account.m_UserID = Result.m_Data.m_Login.m_UserID;
			str_copy(m_Account.m_Username, Result.m_Data.m_Register.m_Username, 32);
			m_Account.m_Authenticated = true;
			m_Account.m_Experience = Result.m_Data.m_Login.m_Experience;
			m_Account.m_Level = Result.m_Data.m_Login.m_Level;
			m_Score = m_Account.m_Level;

			ShowLevelProgress();

			if (Result.m_Data.m_Login.m_RconLevel > 0)
				((CServer*) Server())->ForceAuth(m_ClientID, Result.m_Data.m_Login.m_RconLevel);

			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "You have successfully logged in as %s (id #%d)!", m_Account.m_Username, m_Account.m_UserID);
			GameServer()->SendChatTarget(m_ClientID, aBuf);

			for (int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
			{
				if (ClientID == m_ClientID)
					continue;

				if (!GameServer()->m_apPlayers[ClientID] || !GameServer()->IsClientPlayer(ClientID))
					continue;

				if (!GameServer()->m_apPlayers[ClientID]->m_Account.m_Authenticated)
					continue;

				if (str_comp(GameServer()->m_apPlayers[ClientID]->m_Account.m_Username, m_Account.m_Username) == 0)
					GameServer()->Score()->Logout(ClientID);
			}
			break;
		}
		case CScoreAuthResult::CHANGE_PASSWORD:
		{
			if (!Result.m_Data.m_ChangePassword.m_Success)
			{
				GameServer()->SendChatTarget(m_ClientID, "Error! It's not possible to process your request now");
				break;
			}

			GameServer()->SendChatTarget(m_ClientID, "You have successfully changed your password");
			break;
		}
		case CScoreAuthResult::LOGOUT:
		{
			if (!Result.m_Data.m_Logout.m_Success)
			{
				GameServer()->SendChatTarget(m_ClientID, "Error! It's not possible to process your request now");
				break;
			}

			((CServer*) Server())->LogoutClient(m_ClientID, "/logout");

			m_Account.m_Authenticated = false;
			m_Score = 0;

			GameServer()->SendChatTarget(m_ClientID, "You are logged out");
			break;
		}
		case CScoreAuthResult::LINK_DISCORD:
		{
			if (Result.m_Data.m_LinkDiscord.m_Linked)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "You have already linked this account (%s #%d) to Discord account (id: %lu)", m_Account.m_Username, m_Account.m_UserID, Result.m_Data.m_LinkDiscord.m_DiscordID);
				GameServer()->SendChatTarget(m_ClientID, aBuf);
				return;
			}

			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), g_Config.m_SvDiscordLinkMessage, Result.m_Data.m_LinkDiscord.m_Code, Result.m_Data.m_LinkDiscord.m_Lifetime);
			GameServer()->SendChatTarget(m_ClientID, aBuf);
			break;
		}
	}
}

void CPlayer::ProcessExperienceResult(CScoreExperienceResult &Result)
{
	if(!m_Account.m_Authenticated)
	{
		ShowLevelProgress(); // show tip to create account
		return;
	}

	if(Result.m_Done)
	{
		if (Result.m_Level < 0)
		{
			GameServer()->SendChatTarget(m_ClientID, "Error! Can't give experience");
			return;
		}

		if (Result.m_LevelUp)
		{
			GameServer()->CreateSoundGlobal(SOUND_CTF_CAPTURE, m_ClientID);
			if (m_pCharacter)
				m_pCharacter->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
		}

		m_Account.m_Experience = Result.m_Experience;
		m_Account.m_Level = Result.m_Level;
		m_Score = m_Account.m_Level;

		ShowLevelProgress(Result.m_ExperienceIncrement, Result.m_ExperienceMultiplier);
	}
}

void CPlayer::ProcessStatsResult(CScoreStatsResult &Result)
{
	if (!Result.m_Done || !m_Account.m_Authenticated)
		return;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Statistics for %s (id #%d):", m_Account.m_Username, m_Account.m_UserID);
	GameServer()->SendChatTarget(m_ClientID, aBuf);

	str_format(aBuf, sizeof(aBuf), "Level: %d | Experience: %d/%d", Result.m_Level, Result.m_Experience, IScore::ExperienceRequired(Result.m_Level+1));
	GameServer()->SendChatTarget(m_ClientID, aBuf);

	str_format(aBuf, sizeof(aBuf), "Block Kills/Deaths/Ratio: %d/%d/%.1f", Result.m_BlockKills, Result.m_BlockDeaths, (float) Result.m_BlockKills / Result.m_BlockDeaths);
	GameServer()->SendChatTarget(m_ClientID, aBuf);

	str_format(aBuf, sizeof(aBuf), "Races: %d", Result.m_Races);
	GameServer()->SendChatTarget(m_ClientID, aBuf);
}
