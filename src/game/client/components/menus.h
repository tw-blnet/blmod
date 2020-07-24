/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MENUS_H
#define GAME_CLIENT_COMPONENTS_MENUS_H

#include <base/vmath.h>
#include <base/tl/sorted_array.h>

#include <engine/demo.h>
#include <engine/friends.h>
#include <engine/shared/config.h>

#include <game/voting.h>
#include <game/client/component.h>
#include <game/client/ui.h>


// compnent to fetch keypresses, override all other input
class CMenusKeyBinder : public CComponent
{
public:
	bool m_TakeKey;
	bool m_GotKey;
	IInput::CEvent m_Key;
	int m_Modifier;
	CMenusKeyBinder();
	virtual bool OnInput(IInput::CEvent Event);
};

class CMenus : public CComponent
{
	static ColorRGBA ms_GuiColor;
	static ColorRGBA ms_ColorTabbarInactiveOutgame;
	static ColorRGBA ms_ColorTabbarActiveOutgame;
	static ColorRGBA ms_ColorTabbarInactiveIngame;
	static ColorRGBA ms_ColorTabbarActiveIngame;
	static ColorRGBA ms_ColorTabbarInactive;
	static ColorRGBA ms_ColorTabbarActive;

	float ButtonColorMul(const void *pID);


	int DoButton_DemoPlayer(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoButton_Sprite(const void *pID, int ImageID, int SpriteID, int Checked, const CUIRect *pRect, int Corners);
	int DoButton_Toggle(const void *pID, int Checked, const CUIRect *pRect, bool Active);
	int DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoButton_MenuTab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Corners);

	int DoButton_CheckBox_Common(const void *pID, const char *pText, const char *pBoxText, const CUIRect *pRect);
	int DoButton_CheckBox(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoButton_CheckBox_Number(const void *pID, const char *pText, int Checked, const CUIRect *pRect);

	/*static void ui_draw_menu_button(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	static void ui_draw_keyselect_button(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	static void ui_draw_menu_tab_button(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	static void ui_draw_settings_tab_button(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	*/

	int DoButton_Icon(int ImageId, int SpriteId, const CUIRect *pRect);
	int DoButton_GridHeader(const void *pID, const char *pText, int Checked, const CUIRect *pRect);

	//static void ui_draw_browse_icon(int what, const CUIRect *r);
	//static void ui_draw_grid_header(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);

	/*static void ui_draw_checkbox_common(const void *id, const char *text, const char *boxtext, const CUIRect *r, const void *extra);
	static void ui_draw_checkbox(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	static void ui_draw_checkbox_number(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	*/
	int DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden=false, int Corners=CUI::CORNER_ALL, const char *pEmptyText = "");
	int DoClearableEditBox(void *pID, void *pClearID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden=false, int Corners=CUI::CORNER_ALL, const char *pEmptyText = "");
	//static int ui_do_edit_box(void *id, const CUIRect *rect, char *str, unsigned str_size, float font_size, bool hidden=false);

	float DoScrollbarV(const void *pID, const CUIRect *pRect, float Current);
	float DoScrollbarH(const void *pID, const CUIRect *pRect, float Current);
	void DoButton_KeySelect(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoKeyReader(void *pID, const CUIRect *pRect, int Key, int Modifier, int *NewModifier);

	//static int ui_do_key_reader(void *id, const CUIRect *rect, int key);
	void UiDoGetButtons(int Start, int Stop, CUIRect View, CUIRect ScopeView);

	struct CListboxItem
	{
		int m_Visible;
		int m_Selected;
		CUIRect m_Rect;
		CUIRect m_HitRect;
	};

	void UiDoListboxStart(const void *pID, const CUIRect *pRect, float RowHeight, const char *pTitle, const char *pBottomText, int NumItems,
						int ItemsPerRow, int SelectedIndex, float ScrollValue);
	CListboxItem UiDoListboxNextItem(const void *pID, bool Selected = false, bool KeyEvents = true);
	CListboxItem UiDoListboxNextRow();
	int UiDoListboxEnd(float *pScrollValue, bool *pItemActivated, bool *pListBoxActive = 0);

	//static void demolist_listdir_callback(const char *name, int is_dir, void *user);
	//static void demolist_list_callback(const CUIRect *rect, int index, void *user);

	int m_GamePage;
	int m_Popup;
	int m_ActivePage;
	bool m_MenuActive;
	bool m_UseMouseButtons;
	vec2 m_MousePos;
	bool m_MouseSlow;

	int64 m_LastInput;

	// loading
	int m_LoadCurrent;
	int m_LoadTotal;

	//
	char m_aMessageTopic[512];
	char m_aMessageBody[512];
	char m_aMessageButton[512];

	void PopupMessage(const char *pTopic, const char *pBody, const char *pButton);

	// TODO: this is a bit ugly but.. well.. yeah
	enum { MAX_INPUTEVENTS = 32 };
	static IInput::CEvent m_aInputEvents[MAX_INPUTEVENTS];
	static int m_NumInputEvents;

	// some settings
	static float ms_ButtonHeight;
	static float ms_ListheaderHeight;
	static float ms_ListitemAdditionalHeight;
	static float ms_FontmodHeight;

	// for settings
	bool m_NeedRestartGeneral;
	bool m_NeedRestartSkins;
	bool m_NeedRestartGraphics;
	bool m_NeedRestartSound;
	bool m_NeedRestartUpdate;
	bool m_NeedRestartDDNet;
	bool m_NeedSendinfo;
	bool m_NeedSendDummyinfo;
	int m_SettingPlayerPage;

	//
	bool m_EscapePressed;
	bool m_EnterPressed;
	bool m_DeletePressed;

	// for map download popup
	int64 m_DownloadLastCheckTime;
	int m_DownloadLastCheckSize;
	float m_DownloadSpeed;

	// for call vote
	int m_CallvoteSelectedOption;
	int m_CallvoteSelectedPlayer;
	char m_aCallvoteReason[VOTE_REASON_LENGTH];
	char m_aFilterString[25];

	// demo
	enum
	{
		SORT_DEMONAME=0,
		SORT_MARKERS,
		SORT_LENGTH,
		SORT_DATE,
	};

	struct CDemoItem
	{
		char m_aFilename[128];
		char m_aName[128];
		bool m_IsDir;
		int m_StorageType;
		time_t m_Date;

		bool m_InfosLoaded;
		bool m_Valid;
		CDemoHeader m_Info;
		CTimelineMarkers m_TimelineMarkers;
		CMapInfo m_MapInfo;

		int NumMarkers() const
		{
			return ((m_TimelineMarkers.m_aNumTimelineMarkers[0]<<24)&0xFF000000) | ((m_TimelineMarkers.m_aNumTimelineMarkers[1]<<16)&0xFF0000) |
				((m_TimelineMarkers.m_aNumTimelineMarkers[2]<<8)&0xFF00) | (m_TimelineMarkers.m_aNumTimelineMarkers[3]&0xFF);
		}

		int Length() const
		{
			return ((m_Info.m_aLength[0]<<24)&0xFF000000) | ((m_Info.m_aLength[1]<<16)&0xFF0000) |
				((m_Info.m_aLength[2]<<8)&0xFF00) | (m_Info.m_aLength[3]&0xFF);
		}

		bool operator<(const CDemoItem &Other) const
		{
			if(!str_comp(m_aFilename, ".."))
				return true;
			if(!str_comp(Other.m_aFilename, ".."))
				return false;
			if(m_IsDir && !Other.m_IsDir)
				return true;
			if(!m_IsDir && Other.m_IsDir)
				return false;

			const CDemoItem &Left = g_Config.m_BrDemoSortOrder ? Other : *this;
			const CDemoItem &Right = g_Config.m_BrDemoSortOrder ? *this : Other;

			if(g_Config.m_BrDemoSort == SORT_DEMONAME)
				return str_comp_nocase(Left.m_aFilename, Right.m_aFilename) < 0;
			if(g_Config.m_BrDemoSort == SORT_DATE)
				return Left.m_Date < Right.m_Date;

			if(!Other.m_InfosLoaded)
				return m_InfosLoaded;
			if(!m_InfosLoaded)
				return !Other.m_InfosLoaded;

			if(g_Config.m_BrDemoSort == SORT_MARKERS)
				return Left.NumMarkers() < Right.NumMarkers();
			if(g_Config.m_BrDemoSort == SORT_LENGTH)
				return Left.Length() < Right.Length();

			// Unknown sort
			return true;
		}
	};

	//sorted_array<CDemoItem> m_lDemos;
	char m_aCurrentDemoFolder[256];
	char m_aCurrentDemoFile[64];
	int m_DemolistSelectedIndex;
	bool m_DemolistSelectedIsDir;
	int m_DemolistStorageType;
	int m_Speed = 4;

	void DemolistOnUpdate(bool Reset);
	//void DemolistPopulate();
	static int DemolistFetchCallback(const char *pName, time_t Date, int IsDir, int StorageType, void *pUser);

	// friends
	struct CFriendItem
	{
		const CFriendInfo *m_pFriendInfo;
		int m_NumFound;

		bool operator<(const CFriendItem &Other)
		{
			if(m_NumFound && !Other.m_NumFound)
				return true;
			else if(!m_NumFound && Other.m_NumFound)
				return false;
			else
			{
				int Result = str_comp(m_pFriendInfo->m_aName, Other.m_pFriendInfo->m_aName);
				if(Result)
					return Result < 0;
				else
					return str_comp(m_pFriendInfo->m_aClan, Other.m_pFriendInfo->m_aClan) < 0;
			}
		}
	};

	sorted_array<CFriendItem> m_lFriends;
	int m_FriendlistSelectedIndex;

	void FriendlistOnUpdate();

	// found in menus.cpp
	int Render();
	//void render_background();
	//void render_loading(float percent);
	int RenderMenubar(CUIRect r);
	void RenderNews(CUIRect MainView);

	// found in menus_demo.cpp
	static bool DemoFilterChat(const void *pData, int Size, void *pUser);
	bool FetchHeader(CDemoItem &Item);
	void FetchAllHeaders();
	void RenderDemoPlayer(CUIRect MainView);
	void RenderDemoList(CUIRect MainView);

	// found in menus_ingame.cpp
	void RenderGame(CUIRect MainView);
	void RenderPlayers(CUIRect MainView);
	void RenderServerInfo(CUIRect MainView);
	void RenderServerControl(CUIRect MainView);
	bool RenderServerControlKick(CUIRect MainView, bool FilterSpectators);
	bool RenderServerControlServer(CUIRect MainView);

	// found in menus_browser.cpp
	int m_SelectedIndex;
	int m_DoubleClickIndex;
	int m_ScrollOffset;
	void RenderServerbrowserServerList(CUIRect View);
	void RenderServerbrowserServerDetail(CUIRect View);
	void RenderServerbrowserFilters(CUIRect View);
	void RenderServerbrowserFriends(CUIRect View);
	void RenderServerbrowser(CUIRect MainView);
	static void ConchainFriendlistUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainServerbrowserUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	// found in menus_settings.cpp
	void RenderLanguageSelection(CUIRect MainView);
	void RenderSettingsGeneral(CUIRect MainView);
	void RenderSettingsPlayer(CUIRect MainView);
	void RenderSettingsDummyPlayer(CUIRect MainView);
	void RenderSettingsTee(CUIRect MainView);
	void RenderSettingsControls(CUIRect MainView);
	void RenderSettingsGraphics(CUIRect MainView);
	void RenderSettingsSound(CUIRect MainView);
	void RenderSettings(CUIRect MainView);

	void SetActive(bool Active);

	IGraphics::CTextureHandle m_TextureBlob;
public:
	void RenderBackground();

	void UseMouseButtons(bool Use) { m_UseMouseButtons = Use; }

	static CMenusKeyBinder m_Binder;

	CMenus();

	void RenderLoading();
	void RenderUpdating(const char *pCaption, int current=0, int total=0);

	bool IsActive() const { return m_MenuActive; }

	virtual void OnInit();

	virtual void OnStateChange(int NewState, int OldState);
	virtual void OnReset();
	virtual void OnRender();
	virtual bool OnInput(IInput::CEvent Event);
	virtual bool OnMouseMove(float x, float y);

	enum
	{
		PAGE_NEWS=1,
		PAGE_GAME,
		PAGE_PLAYERS,
		PAGE_SERVER_INFO,
		PAGE_CALLVOTE,
		PAGE_INTERNET,
		PAGE_LAN,
		PAGE_FAVORITES,
		PAGE_DDNET,
		PAGE_KOG,
		PAGE_DEMOS,
		PAGE_SETTINGS,
		PAGE_SYSTEM,
		PAGE_NETWORK,
		PAGE_GHOST,

		SETTINGS_LANGUAGE=0,
		SETTINGS_GENERAL,
		SETTINGS_PLAYER,
		SETTINGS_TEE,
		SETTINGS_HUD,
		SETTINGS_CONTROLS,
		SETTINGS_GRAPHICS,
		SETTINGS_SOUND,
		SETTINGS_DDNET,
	};

	// DDRace
	int DoButton_CheckBox_DontCare(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	sorted_array<CDemoItem> m_lDemos;
	void DemolistPopulate();
	bool m_Dummy;

	const char *GetCurrentDemoFolder() const { return m_aCurrentDemoFolder; }

	// Ghost
	struct CGhostItem
	{
		char m_aFilename[256];
		char m_aPlayer[MAX_NAME_LENGTH];

		int m_Time;
		int m_Slot;
		bool m_Own;

		CGhostItem() : m_Slot(-1), m_Own(false) { m_aFilename[0] = 0; }

		bool operator<(const CGhostItem &Other) { return m_Time < Other.m_Time; }

		bool Active() const { return m_Slot != -1; }
		bool HasFile() const { return m_aFilename[0]; }
	};

	sorted_array<CGhostItem> m_lGhosts;

	void GhostlistPopulate();
	CGhostItem *GetOwnGhost();
	void UpdateOwnGhost(CGhostItem Item);
	void DeleteGhostItem(int Index);

	void setPopup(int Popup) { m_Popup = Popup; }

	int m_DemoPlayerState;
	char m_aDemoPlayerPopupHint[256];

	enum
	{
		POPUP_NONE=0,
		POPUP_FIRST_LAUNCH,
		POPUP_CONNECTING,
		POPUP_MESSAGE,
		POPUP_DISCONNECTED,
		POPUP_PURE,
		POPUP_LANGUAGE,
		POPUP_COUNTRY,
		POPUP_DELETE_DEMO,
		POPUP_RENAME_DEMO,
		POPUP_RENDER_DEMO,
		POPUP_REPLACE_VIDEO,
		POPUP_REMOVE_FRIEND,
		POPUP_SOUNDERROR,
		POPUP_PASSWORD,
		POPUP_QUIT,
		POPUP_DISCONNECT,
		POPUP_DISCONNECT_DUMMY,

		// demo player states
		DEMOPLAYER_NONE=0,
		DEMOPLAYER_SLICE_SAVE,
	};

private:

	static int GhostlistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser);

	// found in menus_ingame.cpp
	void RenderInGameNetwork(CUIRect MainView);
	void RenderGhost(CUIRect MainView);

	// found in menus_settings.cpp
	void RenderSettingsDDNet(CUIRect MainView);
	void RenderSettingsHUD(CUIRect MainView);
	ColorHSLA RenderHSLScrollbars(CUIRect *pRect, unsigned int *pColor, bool Alpha = false);
};
#endif
