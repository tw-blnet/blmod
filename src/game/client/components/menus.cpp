/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/tl/array.h>

#include <math.h>

#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include <engine/config.h>
#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include <game/version.h>
#include <game/generated/protocol.h>

#include <game/generated/client_data.h>
#include <game/client/components/binds.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/localization.h>
#include <mastersrv/mastersrv.h>

#include "countryflags.h"
#include "menus.h"
#include "skins.h"
#include "controls.h"

ColorRGBA CMenus::ms_GuiColor;
ColorRGBA CMenus::ms_ColorTabbarInactiveOutgame;
ColorRGBA CMenus::ms_ColorTabbarActiveOutgame;
ColorRGBA CMenus::ms_ColorTabbarInactive;
ColorRGBA CMenus::ms_ColorTabbarActive = ColorRGBA(0,0,0,0.5f);
ColorRGBA CMenus::ms_ColorTabbarInactiveIngame;
ColorRGBA CMenus::ms_ColorTabbarActiveIngame;

float CMenus::ms_ButtonHeight = 25.0f;
float CMenus::ms_ListheaderHeight = 17.0f;
float CMenus::ms_FontmodHeight = 0.8f;

IInput::CEvent CMenus::m_aInputEvents[MAX_INPUTEVENTS];
int CMenus::m_NumInputEvents;


CMenus::CMenus()
{
	m_Popup = POPUP_NONE;
	m_ActivePage = PAGE_INTERNET;
	m_GamePage = PAGE_GAME;

	m_NeedRestartGraphics = false;
	m_NeedRestartSound = false;
	m_NeedSendinfo = false;
	m_NeedSendDummyinfo = false;
	m_MenuActive = true;
	m_UseMouseButtons = true;
	m_MouseSlow = false;

	m_EscapePressed = false;
	m_EnterPressed = false;
	m_DeletePressed = false;
	m_NumInputEvents = 0;

	m_LastInput = time_get();

	str_copy(m_aCurrentDemoFolder, "demos", sizeof(m_aCurrentDemoFolder));
	m_aCallvoteReason[0] = 0;

	m_FriendlistSelectedIndex = -1;
	m_DoubleClickIndex = -1;

	m_DemoPlayerState = DEMOPLAYER_NONE;
	m_Dummy = false;
}

float CMenus::ButtonColorMul(const void *pID)
{
	if(UI()->ActiveItem() == pID)
		return 0.5f;
	else if(UI()->HotItem() == pID)
		return 1.5f;
	return 1;
}

int CMenus::DoButton_Icon(int ImageId, int SpriteId, const CUIRect *pRect)
{
	int x = pRect->x;
	int y = pRect->y;
	int w = pRect->w;
	int h = pRect->h;

	// Square and center
	if(w > h)
	{
		x += (w-h) / 2;
		w = h;
	}
	else if(h > w)
	{
		y += (h-w) / 2;
		h = w;
	}

	Graphics()->TextureSet(g_pData->m_aImages[ImageId].m_Id);

	Graphics()->QuadsBegin();
	RenderTools()->SelectSprite(SpriteId);
	IGraphics::CQuadItem QuadItem(x, y, w, h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	return 0;
}

int CMenus::DoButton_Toggle(const void *pID, int Checked, const CUIRect *pRect, bool Active)
{
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GUIBUTTONS].m_Id);
	Graphics()->QuadsBegin();
	if(!Active)
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);
	RenderTools()->SelectSprite(Checked?SPRITE_GUIBUTTON_ON:SPRITE_GUIBUTTON_OFF);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	if(UI()->HotItem() == pID && Active)
	{
		RenderTools()->SelectSprite(SPRITE_GUIBUTTON_HOVER);
		IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	}
	Graphics()->QuadsEnd();

	return Active ? UI()->DoButtonLogic(pID, "", Checked, pRect) : 0;
}

int CMenus::DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	RenderTools()->DrawUIRect(pRect, ColorRGBA(1,1,1,0.5f * ButtonColorMul(pID)), CUI::CORNER_ALL, 5.0f);
	CUIRect Temp;
	pRect->HMargin(pRect->h>=20.0f?2.0f:1.0f, &Temp);
	UI()->DoLabel(&Temp, pText, Temp.h*ms_FontmodHeight, 0);
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

void CMenus::DoButton_KeySelect(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	RenderTools()->DrawUIRect(pRect, ColorRGBA(1,1,1,0.5f * ButtonColorMul(pID)), CUI::CORNER_ALL, 5.0f);
	CUIRect Temp;
	pRect->HMargin(1.0f, &Temp);
	UI()->DoLabel(&Temp, pText, Temp.h*ms_FontmodHeight, 0);
}

int CMenus::DoButton_MenuTab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Corners)
{
	if(Checked)
		RenderTools()->DrawUIRect(pRect, ms_ColorTabbarActive, Corners, 10.0f);
	else
		RenderTools()->DrawUIRect(pRect, ms_ColorTabbarInactive, Corners, 10.0f);
	CUIRect Temp;
	pRect->HMargin(2.0f, &Temp);
	UI()->DoLabel(&Temp, pText, Temp.h*ms_FontmodHeight, 0);

	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

int CMenus::DoButton_GridHeader(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	if(Checked)
		RenderTools()->DrawUIRect(pRect, ColorRGBA(1,1,1,0.5f), CUI::CORNER_T, 5.0f);
	CUIRect t;
	pRect->VSplitLeft(5.0f, 0, &t);
	UI()->DoLabel(&t, pText, pRect->h*ms_FontmodHeight, -1);
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

int CMenus::DoButton_CheckBox_Common(const void *pID, const char *pText, const char *pBoxText, const CUIRect *pRect)
{
	CUIRect c = *pRect;
	CUIRect t = *pRect;
	c.w = c.h;
	t.x += c.w;
	t.w -= c.w;
	t.VSplitLeft(5.0f, 0, &t);

	c.Margin(2.0f, &c);
	RenderTools()->DrawUIRect(&c, ColorRGBA(1,1,1,0.25f * ButtonColorMul(pID)), CUI::CORNER_ALL, 3.0f);

	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT);
	bool CheckAble = *pBoxText == 'X';
	if(CheckAble)
	{
		TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
		UI()->DoLabel(&c, "\xEE\x97\x8D", c.h*ms_FontmodHeight, 0);
		TextRender()->SetCurFont(NULL);
	}
	else
		UI()->DoLabel(&c, pBoxText, c.h*ms_FontmodHeight, 0);
	TextRender()->SetRenderFlags(0);
	UI()->DoLabel(&t, pText, c.h*ms_FontmodHeight, -1);

	return UI()->DoButtonLogic(pID, pText, 0, pRect);
}

int CMenus::DoButton_CheckBox(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	return DoButton_CheckBox_Common(pID, pText, Checked?"X":"", pRect);
}


int CMenus::DoButton_CheckBox_Number(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d", Checked);
	return DoButton_CheckBox_Common(pID, pText, aBuf, pRect);
}

int CMenus::DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden, int Corners, const char *pEmptyText)
{
	int Inside = UI()->MouseInside(pRect);
	bool ReturnValue = false;
	bool UpdateOffset = false;
	static int s_AtIndex = 0;
	static bool s_DoScroll = false;
	static float s_ScrollStart = 0.0f;

	FontSize *= UI()->Scale();

	if(UI()->LastActiveItem() == pID)
	{
		int Len = str_length(pStr);
		if(Len == 0)
			s_AtIndex = 0;

		if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyPress(KEY_V))
		{
			const char *Text = Input()->GetClipboardText();
			if(Text)
			{
				int Offset = str_length(pStr);
				int CharsLeft = StrSize - Offset - 1;
				for(int i = 0; i < str_length(Text) && i < CharsLeft; i++)
				{
					if(Text[i] == '\n')
						pStr[i + Offset] = ' ';
					else
						pStr[i + Offset] = Text[i];
				}
				s_AtIndex = str_length(pStr);
				ReturnValue = true;
			}
		}

		if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyPress(KEY_C))
		{
			Input()->SetClipboardText(pStr);
		}

		if(Inside && UI()->MouseButton(0))
		{
			s_DoScroll = true;
			s_ScrollStart = UI()->MouseX();
			int MxRel = (int)(UI()->MouseX() - pRect->x);

			for(int i = 1; i <= Len; i++)
			{
				if(TextRender()->TextWidth(0, FontSize, pStr, i) - *Offset > MxRel)
				{
					s_AtIndex = i - 1;
					break;
				}

				if(i == Len)
					s_AtIndex = Len;
			}
		}
		else if(!UI()->MouseButton(0))
			s_DoScroll = false;
		else if(s_DoScroll)
		{
			// do scrolling
			if(UI()->MouseX() < pRect->x && s_ScrollStart-UI()->MouseX() > 10.0f)
			{
				s_AtIndex = maximum(0, s_AtIndex-1);
				s_ScrollStart = UI()->MouseX();
				UpdateOffset = true;
			}
			else if(UI()->MouseX() > pRect->x+pRect->w && UI()->MouseX()-s_ScrollStart > 10.0f)
			{
				s_AtIndex = minimum(Len, s_AtIndex+1);
				s_ScrollStart = UI()->MouseX();
				UpdateOffset = true;
			}
		}

		for(int i = 0; i < m_NumInputEvents; i++)
		{
			Len = str_length(pStr);
			int NumChars = Len;
			ReturnValue |= CLineInput::Manipulate(m_aInputEvents[i], pStr, StrSize, StrSize, &Len, &s_AtIndex, &NumChars);
		}
	}

	bool JustGotActive = false;

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
		{
			s_AtIndex = minimum(s_AtIndex, str_length(pStr));
			s_DoScroll = false;
			UI()->SetActiveItem(0);
		}
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			if(UI()->LastActiveItem() != pID)
				JustGotActive = true;
			UI()->SetActiveItem(pID);
		}
	}

	if(Inside)
	{
		UI()->SetHotItem(pID);
	}

	CUIRect Textbox = *pRect;
	RenderTools()->DrawUIRect(&Textbox, ColorRGBA(1, 1, 1, 0.5f), Corners, 3.0f);
	Textbox.VMargin(2.0f, &Textbox);
	Textbox.HMargin(2.0f, &Textbox);

	const char *pDisplayStr = pStr;
	char aStars[128];

	if(pDisplayStr[0] == '\0')
	{
		pDisplayStr = pEmptyText;
		TextRender()->TextColor(1, 1, 1, 0.75f);
	}

	if(Hidden)
	{
		unsigned s = str_length(pDisplayStr);
		if(s >= sizeof(aStars))
			s = sizeof(aStars)-1;
		for(unsigned int i = 0; i < s; ++i)
			aStars[i] = '*';
		aStars[s] = 0;
		pDisplayStr = aStars;
	}

	char aInputing[32] = {0};
	if(UI()->HotItem() == pID && Input()->GetIMEState())
	{
		str_copy(aInputing, pStr, sizeof(aInputing));
		const char *Text = Input()->GetIMECandidate();
		if(str_length(Text))
		{
		int NewTextLen = str_length(Text);
		int CharsLeft = StrSize - str_length(aInputing) - 1;
		int FillCharLen = minimum(NewTextLen, CharsLeft);
		//Push Char Backward
		for(int i = str_length(aInputing); i >= s_AtIndex ; i--)
			aInputing[i+FillCharLen] = aInputing[i];
		for(int i = 0; i < FillCharLen; i++)
		{
			if(Text[i] == '\n')
				aInputing[s_AtIndex + i] = ' ';
			else
				aInputing[s_AtIndex + i] = Text[i];
		}
		//s_AtIndex = s_AtIndex+FillCharLen;
		pDisplayStr = aInputing;
		}
	}

	// check if the text has to be moved
	if(UI()->LastActiveItem() == pID && !JustGotActive && (UpdateOffset || m_NumInputEvents))
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, s_AtIndex);
		if(w-*Offset > Textbox.w)
		{
			// move to the left
			float wt = TextRender()->TextWidth(0, FontSize, pDisplayStr, -1);
			do
			{
				*Offset += minimum(wt-*Offset-Textbox.w, Textbox.w/3);
			}
			while(w-*Offset > Textbox.w);
		}
		else if(w-*Offset < 0.0f)
		{
			// move to the right
			do
			{
				*Offset = maximum(0.0f, *Offset-Textbox.w/3);
			}
			while(w-*Offset < 0.0f);
		}
	}
	UI()->ClipEnable(pRect);
	Textbox.x -= *Offset;

	UI()->DoLabel(&Textbox, pDisplayStr, FontSize, -1);

	TextRender()->TextColor(1, 1, 1, 1);

	// render the cursor
	if(UI()->LastActiveItem() == pID && !JustGotActive)
	{
		if(str_length(aInputing))
		{
			float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, s_AtIndex + Input()->GetEditingCursor());
			Textbox = *pRect;
			Textbox.VSplitLeft(2.0f, 0, &Textbox);
			Textbox.x += (w-*Offset-TextRender()->TextWidth(0, FontSize, "|", -1)/2);

			UI()->DoLabel(&Textbox, "|", FontSize, -1);
		}
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, s_AtIndex);
		Textbox = *pRect;
		Textbox.VSplitLeft(2.0f, 0, &Textbox);
		Textbox.x += (w-*Offset-TextRender()->TextWidth(0, FontSize, "|", -1)/2);

		if((2*time_get()/time_freq()) % 2)	// make it blink
			UI()->DoLabel(&Textbox, "|", FontSize, -1);
	}
	UI()->ClipDisable();

	return ReturnValue;
}

int CMenus::DoClearableEditBox(void *pID, void *pClearID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden, int Corners, const char *pEmptyText)
{
	bool ReturnValue = false;
	CUIRect EditBox;
	CUIRect ClearButton;
	pRect->VSplitRight(15.0f, &EditBox, &ClearButton);
	if(DoEditBox(pID, &EditBox, pStr, StrSize, FontSize, Offset, Hidden, Corners&~CUI::CORNER_R, pEmptyText))
	{
		ReturnValue = true;
	}

	RenderTools()->DrawUIRect(&ClearButton, ColorRGBA(1, 1, 1, 0.33f * ButtonColorMul(pID)), Corners&~CUI::CORNER_L, 3.0f);
	UI()->DoLabel(&ClearButton, "×", ClearButton.h * ms_FontmodHeight, 0);
	if(UI()->DoButtonLogic(pClearID, "×", 0, &ClearButton))
	{
		pStr[0] = 0;
		UI()->SetActiveItem(pID);
		ReturnValue = true;
	}
	return ReturnValue;
}

float CMenus::DoScrollbarV(const void *pID, const CUIRect *pRect, float Current)
{
	CUIRect Handle;
	static float OffsetY;
	pRect->HSplitTop(33, &Handle, 0);

	Handle.y += (pRect->h-Handle.h)*Current;

	// logic
	float ReturnValue = Current;
	int Inside = UI()->MouseInside(&Handle);

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);

		if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
			m_MouseSlow = true;

		float Min = pRect->y;
		float Max = pRect->h-Handle.h;
		float Cur = UI()->MouseY()-OffsetY;
		ReturnValue = (Cur-Min)/Max;
		if(ReturnValue < 0.0f) ReturnValue = 0.0f;
		if(ReturnValue > 1.0f) ReturnValue = 1.0f;
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			OffsetY = UI()->MouseY()-Handle.y;
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// render
	CUIRect Rail;
	pRect->VMargin(5.0f, &Rail);
	RenderTools()->DrawUIRect(&Rail, ColorRGBA(1,1,1,0.25f), 0, 0.0f);

	CUIRect Slider = Handle;
	Slider.w = Rail.x-Slider.x;
	RenderTools()->DrawUIRect(&Slider, ColorRGBA(1,1,1,0.25f), CUI::CORNER_L, 2.5f);
	Slider.x = Rail.x+Rail.w;
	RenderTools()->DrawUIRect(&Slider, ColorRGBA(1,1,1,0.25f), CUI::CORNER_R, 2.5f);

	Slider = Handle;
	Slider.Margin(5.0f, &Slider);
	RenderTools()->DrawUIRect(&Slider, ColorRGBA(1,1,1,0.25f*ButtonColorMul(pID)), CUI::CORNER_ALL, 2.5f);

	return ReturnValue;
}



float CMenus::DoScrollbarH(const void *pID, const CUIRect *pRect, float Current)
{
	CUIRect Handle;
	static float OffsetX;
	pRect->VSplitLeft(33, &Handle, 0);

	Handle.x += (pRect->w-Handle.w)*Current;

	// logic
	float ReturnValue = Current;
	int Inside = UI()->MouseInside(&Handle);

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);

		if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
			m_MouseSlow = true;

		float Min = pRect->x;
		float Max = pRect->w-Handle.w;
		float Cur = UI()->MouseX()-OffsetX;
		ReturnValue = (Cur-Min)/Max;
		if(ReturnValue < 0.0f) ReturnValue = 0.0f;
		if(ReturnValue > 1.0f) ReturnValue = 1.0f;
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			OffsetX = UI()->MouseX()-Handle.x;
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// render
	CUIRect Rail;
	pRect->HMargin(5.0f, &Rail);
	RenderTools()->DrawUIRect(&Rail, ColorRGBA(1,1,1,0.25f), 0, 0.0f);

	CUIRect Slider = Handle;
	Slider.h = Rail.y-Slider.y;
	RenderTools()->DrawUIRect(&Slider, ColorRGBA(1,1,1,0.25f), CUI::CORNER_T, 2.5f);
	Slider.y = Rail.y+Rail.h;
	RenderTools()->DrawUIRect(&Slider, ColorRGBA(1,1,1,0.25f), CUI::CORNER_B, 2.5f);

	Slider = Handle;
	Slider.Margin(5.0f, &Slider);
	RenderTools()->DrawUIRect(&Slider, ColorRGBA(1,1,1,0.25f * ButtonColorMul(pID)), CUI::CORNER_ALL, 2.5f);

	return ReturnValue;
}

int CMenus::DoKeyReader(void *pID, const CUIRect *pRect, int Key, int Modifier, int *NewModifier)
{
	// process
	static void *pGrabbedID = 0;
	static bool MouseReleased = true;
	static int ButtonUsed = 0;
	int Inside = UI()->MouseInside(pRect);
	int NewKey = Key;
	*NewModifier = Modifier;

	if(!UI()->MouseButton(0) && !UI()->MouseButton(1) && pGrabbedID == pID)
		MouseReleased = true;

	if(UI()->ActiveItem() == pID)
	{
		if(m_Binder.m_GotKey)
		{
			// abort with escape key
			if(m_Binder.m_Key.m_Key != KEY_ESCAPE)
			{
				NewKey = m_Binder.m_Key.m_Key;
				*NewModifier = m_Binder.m_Modifier;
			}
			m_Binder.m_GotKey = false;
			UI()->SetActiveItem(0);
			MouseReleased = false;
			pGrabbedID = pID;
		}

		if(ButtonUsed == 1 && !UI()->MouseButton(1))
		{
			if(Inside)
				NewKey = 0;
			UI()->SetActiveItem(0);
		}
	}
	else if(UI()->HotItem() == pID)
	{
		if(MouseReleased)
		{
			if(UI()->MouseButton(0))
			{
				m_Binder.m_TakeKey = true;
				m_Binder.m_GotKey = false;
				UI()->SetActiveItem(pID);
				ButtonUsed = 0;
			}

			if(UI()->MouseButton(1))
			{
				UI()->SetActiveItem(pID);
				ButtonUsed = 1;
			}
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// draw
	if(UI()->ActiveItem() == pID && ButtonUsed == 0)
		DoButton_KeySelect(pID, "???", 0, pRect);
	else
	{
		if(Key)
		{
			char aBuf[64];
			if(*NewModifier)
				str_format(aBuf, sizeof(aBuf), "%s+%s", CBinds::GetModifierName(*NewModifier), Input()->KeyName(Key));
			else
				str_format(aBuf, sizeof(aBuf), "%s", Input()->KeyName(Key));

			DoButton_KeySelect(pID, aBuf, 0, pRect);
		}
		else
			DoButton_KeySelect(pID, "", 0, pRect);
	}
	return NewKey;
}


int CMenus::RenderMenubar(CUIRect r)
{
	CUIRect Box = r;
	CUIRect Button;

	m_ActivePage = g_Config.m_UiPage;
	int NewPage = -1;

	if(Client()->State() != IClient::STATE_OFFLINE)
		m_ActivePage = m_GamePage;

	if(Client()->State() == IClient::STATE_OFFLINE)
	{
		// offline menus
		Box.VSplitLeft(60.0f, &Button, &Box);
		static int s_NewsButton=0;
		if(DoButton_MenuTab(&s_NewsButton, Localize("News"), m_ActivePage==PAGE_NEWS, &Button, CUI::CORNER_TL))
		{
			NewPage = PAGE_NEWS;
			m_DoubleClickIndex = -1;
		}
		Box.VSplitLeft(60.0f, &Button, &Box);
		static int s_LearnButton=0;
		if(DoButton_MenuTab(&s_LearnButton, Localize("Learn"), false, &Button, CUI::CORNER_TR))
		{
			if(!open_link("https://wiki.ddnet.tw/"))
			{
				dbg_msg("menus", "couldn't open link");
			}
			m_DoubleClickIndex = -1;
		}

		Box.VSplitLeft(5.0f, 0, &Box);

		Box.VSplitLeft(100.0f, &Button, &Box);
		static int s_InternetButton=0;
		if(DoButton_MenuTab(&s_InternetButton, Localize("Internet"), m_ActivePage==PAGE_INTERNET, &Button, CUI::CORNER_TL))
		{
			if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_INTERNET)
				ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
			NewPage = PAGE_INTERNET;
			m_DoubleClickIndex = -1;
		}

		Box.VSplitLeft(60.0f, &Button, &Box);
		static int s_LanButton=0;
		if(DoButton_MenuTab(&s_LanButton, Localize("LAN"), m_ActivePage==PAGE_LAN, &Button, 0))
		{
			if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_LAN)
				ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
			NewPage = PAGE_LAN;
			m_DoubleClickIndex = -1;
		}

		Box.VSplitLeft(100.0f, &Button, &Box);
		static int s_FavoritesButton=0;
		if(DoButton_MenuTab(&s_FavoritesButton, Localize("Favorites"), m_ActivePage==PAGE_FAVORITES, &Button, 0))
		{
			if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_FAVORITES)
				ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
			NewPage = PAGE_FAVORITES;
			m_DoubleClickIndex = -1;
		}

		Box.VSplitLeft(80.0f, &Button, &Box);
		static int s_DDNetButton=0;
		if(DoButton_MenuTab(&s_DDNetButton, "DDNet", m_ActivePage==PAGE_DDNET, &Button, 0))
		{
			if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_DDNET)
			{
				Client()->RequestDDNetInfo();
				ServerBrowser()->Refresh(IServerBrowser::TYPE_DDNET);
			}
			NewPage = PAGE_DDNET;
			m_DoubleClickIndex = -1;
		}

		Box.VSplitLeft(60.0f, &Button, &Box);
		static int s_KoGButton=0;
		if(DoButton_MenuTab(&s_KoGButton, "KoG", m_ActivePage==PAGE_KOG, &Button, CUI::CORNER_TR))
		{
			if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_KOG)
			{
				Client()->RequestDDNetInfo();
				ServerBrowser()->Refresh(IServerBrowser::TYPE_KOG);
			}
			NewPage = PAGE_KOG;
			m_DoubleClickIndex = -1;
		}

		Box.VSplitLeft(5.0f, 0, &Box);
		Box.VSplitLeft(80.0f, &Button, &Box);
		static int s_DemosButton=0;
		if(DoButton_MenuTab(&s_DemosButton, Localize("Demos"), m_ActivePage==PAGE_DEMOS, &Button, CUI::CORNER_T))
		{
			DemolistPopulate();
			NewPage = PAGE_DEMOS;
			m_DoubleClickIndex = -1;
		}
	}
	else
	{
		// online menus
		Box.VSplitLeft(90.0f, &Button, &Box);
		static int s_GameButton=0;
		if(DoButton_MenuTab(&s_GameButton, Localize("Game"), m_ActivePage==PAGE_GAME, &Button, CUI::CORNER_TL))
			NewPage = PAGE_GAME;

		Box.VSplitLeft(90.0f, &Button, &Box);
		static int s_PlayersButton=0;
		if(DoButton_MenuTab(&s_PlayersButton, Localize("Players"), m_ActivePage==PAGE_PLAYERS, &Button, 0))
			NewPage = PAGE_PLAYERS;

		Box.VSplitLeft(130.0f, &Button, &Box);
		static int s_ServerInfoButton=0;
		if(DoButton_MenuTab(&s_ServerInfoButton, Localize("Server info"), m_ActivePage==PAGE_SERVER_INFO, &Button, 0))
			NewPage = PAGE_SERVER_INFO;

		Box.VSplitLeft(90.0f, &Button, &Box);
		static int s_NetworkButton=0;
		if(DoButton_MenuTab(&s_NetworkButton, Localize("Browser"), m_ActivePage==PAGE_NETWORK, &Button, 0))
			NewPage = PAGE_NETWORK;

		{
			static int s_GhostButton = 0;
			if(GameClient()->m_GameInfo.m_Race)
			{
				Box.VSplitLeft(70.0f, &Button, &Box);
				if(DoButton_MenuTab(&s_GhostButton, Localize("Ghost"), m_ActivePage==PAGE_GHOST, &Button, 0))
					NewPage = PAGE_GHOST;
			}
		}

		Box.VSplitLeft(100.0f, &Button, &Box);
		Box.VSplitLeft(4.0f, 0, &Box);
		static int s_CallVoteButton=0;
		if(DoButton_MenuTab(&s_CallVoteButton, Localize("Call vote"), m_ActivePage==PAGE_CALLVOTE, &Button, CUI::CORNER_TR))
			NewPage = PAGE_CALLVOTE;
	}

	TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

	Box.VSplitRight(33.0f, &Box, &Button);
	static int s_QuitButton=0;
	if(DoButton_MenuTab(&s_QuitButton, "\xEE\x97\x8D", 0, &Button, CUI::CORNER_T))
	{
		if(m_pClient->Editor()->HasUnsavedData() || (Client()->GetCurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
		{
			m_Popup = POPUP_QUIT;
		}
		else
		{
			Client()->Quit();
		}
	}

	Box.VSplitRight(10.0f, &Box, &Button);
	Box.VSplitRight(33.0f, &Box, &Button);
	static int s_SettingsButton=0;

	if(DoButton_MenuTab(&s_SettingsButton, "\xEE\xA2\xB8", m_ActivePage==PAGE_SETTINGS, &Button, CUI::CORNER_T))
		NewPage = PAGE_SETTINGS;

	Box.VSplitRight(10.0f, &Box, &Button);
	Box.VSplitRight(33.0f, &Box, &Button);
	static int s_EditorButton=0;
	if(DoButton_MenuTab(&s_EditorButton, "\xEE\x8F\x89", 0, &Button, CUI::CORNER_T))
	{
		g_Config.m_ClEditor = 1;
	}

	TextRender()->SetRenderFlags(0);
	TextRender()->SetCurFont(NULL);

	if(NewPage != -1)
	{
		if(Client()->State() == IClient::STATE_OFFLINE)
			g_Config.m_UiPage = NewPage;
		else
			m_GamePage = NewPage;
	}

	return 0;
}

void CMenus::RenderLoading()
{
	// TODO: not supported right now due to separate render thread

	static int64 LastLoadRender = 0;
	float Percent = m_LoadCurrent++/(float)m_LoadTotal;

	// make sure that we don't render for each little thing we load
	// because that will slow down loading if we have vsync
	if(time_get()-LastLoadRender < time_freq()/60)
		return;

	LastLoadRender = time_get();

	// need up date this here to get correct
	ms_GuiColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_UiColor, true));

	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	RenderBackground();

	float w = 700;
	float h = 200;
	float x = Screen.w/2-w/2;
	float y = Screen.h/2-h/2;

	Graphics()->BlendNormal();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0,0,0,0.50f);
	RenderTools()->DrawRoundRect(x, y, w, h, 40.0f);
	Graphics()->QuadsEnd();


	const char *pCaption = Localize("Loading DDNet Client");

	CUIRect r;
	r.x = x;
	r.y = y+20;
	r.w = w;
	r.h = h - 130;
	UI()->DoLabel(&r, pCaption, 48.0f, 0, -1);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1,1,1,0.75f);
	RenderTools()->DrawRoundRect(x+40, y+h-75, (w-80)*Percent, 25, 5.0f);
	Graphics()->QuadsEnd();

	Graphics()->Swap();
}

void CMenus::RenderNews(CUIRect MainView)
{
	RenderTools()->DrawUIRect(&MainView, ms_ColorTabbarActive, CUI::CORNER_B, 10.0f);

	MainView.HSplitTop(15.0f, 0, &MainView);
	MainView.VSplitLeft(15.0f, 0, &MainView);

	CUIRect Label;

	const char *pStr = Client()->m_aNews;
	char aLine[256];
	while((pStr = str_next_token(pStr, "\n", aLine, sizeof(aLine))))
	{
		const int Len = str_length(aLine);
		if(Len > 0 && aLine[0] == '|' && aLine[Len-1] == '|')
		{
			MainView.HSplitTop(30.0f, &Label, &MainView);
			aLine[Len-1] = '\0';
			UI()->DoLabelScaled(&Label, aLine + 1, 20.0f, -1);
		}
		else
		{
			MainView.HSplitTop(20.0f, &Label, &MainView);
			UI()->DoLabelScaled(&Label, aLine, 15.f, -1, -1);
		}
	}
}

void CMenus::OnInit()
{
	if(g_Config.m_ClShowWelcome)
		m_Popup = POPUP_LANGUAGE;

	Console()->Chain("add_favorite", ConchainServerbrowserUpdate, this);
	Console()->Chain("remove_favorite", ConchainServerbrowserUpdate, this);
	Console()->Chain("add_friend", ConchainFriendlistUpdate, this);
	Console()->Chain("remove_friend", ConchainFriendlistUpdate, this);

	m_TextureBlob = Graphics()->LoadTexture("blob.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);

	// setup load amount
	m_LoadCurrent = 0;
	m_LoadTotal = g_pData->m_NumImages;
	if(!g_Config.m_ClThreadsoundloading)
		m_LoadTotal += g_pData->m_NumSounds;
}

void CMenus::PopupMessage(const char *pTopic, const char *pBody, const char *pButton)
{
	// reset active item
	UI()->SetActiveItem(0);

	str_copy(m_aMessageTopic, pTopic, sizeof(m_aMessageTopic));
	str_copy(m_aMessageBody, pBody, sizeof(m_aMessageBody));
	str_copy(m_aMessageButton, pButton, sizeof(m_aMessageButton));
	m_Popup = POPUP_MESSAGE;
}


int CMenus::Render()
{
	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	m_MouseSlow = false;

	static int s_Frame = 0;
	if(s_Frame == 0)
	{
		s_Frame++;
	}
	else if(s_Frame == 1)
	{
		m_pClient->m_pSounds->Enqueue(CSounds::CHN_MUSIC, SOUND_MENU);
		s_Frame++;
		m_DoubleClickIndex = -1;

		if(g_Config.m_UiPage == PAGE_INTERNET)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
		else if(g_Config.m_UiPage == PAGE_LAN)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
		else if(g_Config.m_UiPage == PAGE_FAVORITES)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
		else if(g_Config.m_UiPage == PAGE_DDNET)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_DDNET);
		else if(g_Config.m_UiPage == PAGE_KOG)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_KOG);
	}

	if(Client()->State() == IClient::STATE_ONLINE)
	{
		ms_ColorTabbarInactive = ms_ColorTabbarInactiveIngame;
		ms_ColorTabbarActive = ms_ColorTabbarActiveIngame;
	}
	else
	{
		RenderBackground();
		ms_ColorTabbarInactive = ms_ColorTabbarInactiveOutgame;
		ms_ColorTabbarActive = ms_ColorTabbarActiveOutgame;
	}

	CUIRect TabBar;
	CUIRect MainView;

	// some margin around the screen
	Screen.Margin(10.0f, &Screen);

	static bool s_SoundCheck = false;
	if(!s_SoundCheck && m_Popup == POPUP_NONE)
	{
		if(Client()->SoundInitFailed())
			m_Popup = POPUP_SOUNDERROR;
		s_SoundCheck = true;
	}

	if(m_Popup == POPUP_NONE)
	{
		Screen.HSplitTop(24.0f, &TabBar, &MainView);

		// render news
		if(g_Config.m_UiPage < PAGE_NEWS || g_Config.m_UiPage > PAGE_SETTINGS || (Client()->State() == IClient::STATE_OFFLINE && g_Config.m_UiPage >= PAGE_GAME && g_Config.m_UiPage <= PAGE_CALLVOTE))
		{
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
			g_Config.m_UiPage = PAGE_INTERNET;
			m_DoubleClickIndex = -1;
		}

		// render current page
		if(Client()->State() != IClient::STATE_OFFLINE)
		{
			if(m_GamePage == PAGE_GAME)
				RenderGame(MainView);
			else if(m_GamePage == PAGE_PLAYERS)
				RenderPlayers(MainView);
			else if(m_GamePage == PAGE_SERVER_INFO)
				RenderServerInfo(MainView);
			else if(m_GamePage == PAGE_NETWORK)
				RenderInGameNetwork(MainView);
			else if(m_GamePage == PAGE_GHOST)
				RenderGhost(MainView);
			else if(m_GamePage == PAGE_CALLVOTE)
				RenderServerControl(MainView);
			else if(m_GamePage == PAGE_SETTINGS)
				RenderSettings(MainView);
		}
		else if(g_Config.m_UiPage == PAGE_NEWS)
			RenderNews(MainView);
		else if(g_Config.m_UiPage == PAGE_INTERNET)
			RenderServerbrowser(MainView);
		else if(g_Config.m_UiPage == PAGE_LAN)
			RenderServerbrowser(MainView);
		else if(g_Config.m_UiPage == PAGE_DEMOS)
			RenderDemoList(MainView);
		else if(g_Config.m_UiPage == PAGE_FAVORITES)
			RenderServerbrowser(MainView);
		else if(g_Config.m_UiPage == PAGE_DDNET)
			RenderServerbrowser(MainView);
		else if(g_Config.m_UiPage == PAGE_KOG)
			RenderServerbrowser(MainView);
		else if(g_Config.m_UiPage == PAGE_SETTINGS)
			RenderSettings(MainView);

		// do tab bar
		RenderMenubar(TabBar);
	}
	else
	{
		// make sure that other windows doesn't do anything funnay!
		//UI()->SetHotItem(0);
		//UI()->SetActiveItem(0);
		char aBuf[1536];
		const char *pTitle = "";
		const char *pExtraText = "";
		const char *pButtonText = "";
		int ExtraAlign = 0;

		if(m_Popup == POPUP_MESSAGE)
		{
			pTitle = m_aMessageTopic;
			pExtraText = m_aMessageBody;
			pButtonText = m_aMessageButton;
		}
		else if(m_Popup == POPUP_CONNECTING)
		{
			pTitle = Localize("Connecting to");
			pExtraText = g_Config.m_UiServerAddress; // TODO: query the client about the address
			pButtonText = Localize("Abort");
			if(Client()->MapDownloadTotalsize() > 0)
			{
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Downloading map"), Client()->MapDownloadName());
				pTitle = aBuf;
				pExtraText = "";
			}
		}
		else if(m_Popup == POPUP_DISCONNECTED)
		{
			pTitle = Localize("Disconnected");
			pExtraText = Client()->ErrorString();
			pButtonText = Localize("Ok");
			if(Client()->m_ReconnectTime > 0)
			{
				str_format(aBuf, sizeof(aBuf), Localize("Reconnect in %d sec"), (int)((Client()->m_ReconnectTime - time_get()) / time_freq()));
				pTitle = Client()->ErrorString();
				pExtraText = aBuf;
				pButtonText = Localize("Abort");
			}
			ExtraAlign = 0;
		}
		else if(m_Popup == POPUP_PURE)
		{
			pTitle = Localize("Disconnected");
			pExtraText = Localize("The server is running a non-standard tuning on a pure game type.");
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_DELETE_DEMO)
		{
			pTitle = Localize("Delete demo");
			pExtraText = Localize("Are you sure that you want to delete the demo?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_RENAME_DEMO)
		{
			pTitle = Localize("Rename demo");
			pExtraText = "";
			ExtraAlign = -1;
		}
	#if defined(CONF_VIDEORECORDER)
		else if(m_Popup == POPUP_RENDER_DEMO)
		{
			pTitle = Localize("Render demo");
			pExtraText = "";
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_REPLACE_VIDEO)
		{
			pTitle = Localize("Replace video");
			pExtraText = Localize("File already exists, do you want to overwrite it?");
			ExtraAlign = -1;
		}
	#endif
		else if(m_Popup == POPUP_REMOVE_FRIEND)
		{
			pTitle = Localize("Remove friend");
			pExtraText = Localize("Are you sure that you want to remove the player from your friends list?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_SOUNDERROR)
		{
			pTitle = Localize("Sound error");
			pExtraText = Localize("The audio device couldn't be initialised.");
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_PASSWORD)
		{
			pTitle = Localize("Password incorrect");
			pExtraText = "";
			pButtonText = Localize("Try again");
		}
		else if(m_Popup == POPUP_QUIT)
		{
			pTitle = Localize("Quit");
			pExtraText = Localize("Are you sure that you want to quit?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_DISCONNECT)
		{
			pTitle = Localize("Disconnect");
			pExtraText = Localize("Are you sure that you want to disconnect?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_DISCONNECT_DUMMY)
		{
			pTitle = Localize("Disconnect Dummy");
			pExtraText = Localize("Are you sure that you want to disconnect your dummy?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_FIRST_LAUNCH)
		{
			pTitle = Localize("Welcome to DDNet");
			str_format(aBuf, sizeof(aBuf), "%s\n\n%s\n\n%s\n\n%s\n\n%s\n\n%s\n\n%s",
				Localize("DDraceNetwork is a cooperative online game where the goal is for you and your group of tees to reach the finish line of the map. As a newcomer you should start on Novice servers, which host the easiest maps. Consider the ping to choose a server close to you."),
				Localize("The maps contain freeze, which temporarily make a tee unable to move. You have to work together to get through these parts."),
				Localize("The mouse wheel changes weapons. Hammer (left mouse) can be used to hit other tees and wake them up from being frozen."),
				Localize("Hook (right mouse) can be used to swing through the map and to hook other tees to you."),
				Localize("Most importantly communication is key: There is no tutorial so you'll have to chat (t key) with other players to learn the basics and tricks of the game."),
				Localize("It's recommended that you check the settings to adjust them to your liking before joining a server."),
				Localize("Please enter your nick name below."));
			pExtraText = aBuf;
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}

		CUIRect Box, Part;
		Box = Screen;
		if(m_Popup != POPUP_FIRST_LAUNCH)
		{
			Box.VMargin(150.0f/UI()->Scale(), &Box);
			Box.HMargin(150.0f/UI()->Scale(), &Box);
		}

		// render the box
		RenderTools()->DrawUIRect(&Box, ColorRGBA(0,0,0,0.5f), CUI::CORNER_ALL, 15.0f);

		Box.HSplitTop(20.f/UI()->Scale(), &Part, &Box);
		Box.HSplitTop(24.f/UI()->Scale(), &Part, &Box);
		Part.VMargin(20.f/UI()->Scale(), &Part);
		if(TextRender()->TextWidth(0, 24.f, pTitle, -1) > Part.w)
			UI()->DoLabelScaled(&Part, pTitle, 24.f, -1, (int)Part.w);
		else
			UI()->DoLabelScaled(&Part, pTitle, 24.f, 0);
		Box.HSplitTop(20.f/UI()->Scale(), &Part, &Box);
		Box.HSplitTop(24.f/UI()->Scale(), &Part, &Box);
		Part.VMargin(20.f/UI()->Scale(), &Part);

		float FontSize = m_Popup == POPUP_FIRST_LAUNCH ? 16.0f : 20.f;

		if(ExtraAlign == -1)
			UI()->DoLabelScaled(&Part, pExtraText, FontSize, -1, (int)Part.w);
		else
		{
			if(TextRender()->TextWidth(0, FontSize, pExtraText, -1) > Part.w)
				UI()->DoLabelScaled(&Part, pExtraText, FontSize, -1, (int)Part.w);
			else
				UI()->DoLabelScaled(&Part, pExtraText, FontSize, 0, -1);
		}

		if(m_Popup == POPUP_QUIT)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			// additional info
			Box.VMargin(20.f/UI()->Scale(), &Box);
			if(m_pClient->Editor()->HasUnsavedData())
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "%s\n%s", Localize("There's an unsaved map in the editor, you might want to save it before you quit the game."), Localize("Quit anyway?"));
				UI()->DoLabelScaled(&Box, aBuf, 20.f, -1, Part.w-20.0f);
			}

			// buttons
			Part.VMargin(80.0f, &Part);
			Part.VSplitMid(&No, &Yes);
			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
				Client()->Quit();
		}
		else if(m_Popup == POPUP_DISCONNECT)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			// buttons
			Part.VMargin(80.0f, &Part);
			Part.VSplitMid(&No, &Yes);
			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
				Client()->Disconnect();
		}
		else if(m_Popup == POPUP_DISCONNECT_DUMMY)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			// buttons
			Part.VMargin(80.0f, &Part);
			Part.VSplitMid(&No, &Yes);
			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
			{
				Client()->DummyDisconnect(0);
				m_Popup = POPUP_NONE;
			}
		}
		else if(m_Popup == POPUP_PASSWORD)
		{
			CUIRect Label, TextBox, TryAgain, Abort;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&Abort, &TryAgain);

			TryAgain.VMargin(20.0f, &TryAgain);
			Abort.VMargin(20.0f, &Abort);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Try again"), 0, &TryAgain) || m_EnterPressed)
			{
				Client()->Connect(g_Config.m_UiServerAddress, g_Config.m_Password);
			}

			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(100.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Password"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&g_Config.m_Password, &TextBox, g_Config.m_Password, sizeof(g_Config.m_Password), 12.0f, &Offset, true);
		}
		else if(m_Popup == POPUP_CONNECTING)
		{
			Box = Screen;
			Box.VMargin(150.0f, &Box);
			Box.HMargin(150.0f, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || m_EscapePressed || m_EnterPressed)
			{
				Client()->Disconnect();
				m_Popup = POPUP_NONE;
			}

			if(Client()->MapDownloadTotalsize() > 0)
			{
				int64 Now = time_get();
				if(Now-m_DownloadLastCheckTime >= time_freq())
				{
					if(m_DownloadLastCheckSize > Client()->MapDownloadAmount())
					{
						// map downloaded restarted
						m_DownloadLastCheckSize = 0;
					}

					// update download speed
					float Diff = (Client()->MapDownloadAmount()-m_DownloadLastCheckSize)/((int)((Now-m_DownloadLastCheckTime)/time_freq()));
					float StartDiff = m_DownloadLastCheckSize-0.0f;
					if(StartDiff+Diff > 0.0f)
						m_DownloadSpeed = (Diff/(StartDiff+Diff))*(Diff/1.0f) + (StartDiff/(Diff+StartDiff))*m_DownloadSpeed;
					else
						m_DownloadSpeed = 0.0f;
					m_DownloadLastCheckTime = Now;
					m_DownloadLastCheckSize = Client()->MapDownloadAmount();
				}

				Box.HSplitTop(64.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				str_format(aBuf, sizeof(aBuf), "%d/%d KiB (%.1f KiB/s)", Client()->MapDownloadAmount()/1024, Client()->MapDownloadTotalsize()/1024,	m_DownloadSpeed/1024.0f);
				UI()->DoLabel(&Part, aBuf, 20.f, 0, -1);

				// time left
				int TimeLeft = maximum(1, m_DownloadSpeed > 0.0f ? static_cast<int>((Client()->MapDownloadTotalsize()-Client()->MapDownloadAmount())/m_DownloadSpeed) : 1);
				if(TimeLeft >= 60)
				{
					TimeLeft /= 60;
					str_format(aBuf, sizeof(aBuf), TimeLeft == 1 ? Localize("%i minute left") : Localize("%i minutes left"), TimeLeft);
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), TimeLeft == 1 ? Localize("%i second left") : Localize("%i seconds left"), TimeLeft);
				}
				Box.HSplitTop(20.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				UI()->DoLabel(&Part, aBuf, 20.f, 0, -1);

				// progress bar
				Box.HSplitTop(20.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				Part.VMargin(40.0f, &Part);
				RenderTools()->DrawUIRect(&Part, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), CUI::CORNER_ALL, 5.0f);
				Part.w = maximum(10.0f, (Part.w*Client()->MapDownloadAmount())/Client()->MapDownloadTotalsize());
				RenderTools()->DrawUIRect(&Part, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f), CUI::CORNER_ALL, 5.0f);
			}
		}
		else if(m_Popup == POPUP_LANGUAGE)
		{
			Box = Screen;
			Box.VMargin(150.0f, &Box);
			Box.HMargin(150.0f, &Box);
			Box.HSplitTop(20.f, &Part, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Box.HSplitBottom(20.f, &Box, 0);
			Box.VMargin(20.0f, &Box);
			RenderLanguageSelection(Box);
			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, Localize("Ok"), 0, &Part) || m_EscapePressed || m_EnterPressed)
				m_Popup = POPUP_FIRST_LAUNCH;
		}
		else if(m_Popup == POPUP_COUNTRY)
		{
			Box = Screen;
			Box.VMargin(150.0f, &Box);
			Box.HMargin(150.0f, &Box);
			Box.HSplitTop(20.f, &Part, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Box.HSplitBottom(20.f, &Box, 0);
			Box.VMargin(20.0f, &Box);

			static int ActSelection = -2;
			if(ActSelection == -2)
				ActSelection = g_Config.m_BrFilterCountryIndex;
			static float s_ScrollValue = 0.0f;
			int OldSelected = -1;
			UiDoListboxStart(&s_ScrollValue, &Box, 50.0f, Localize("Country"), "", m_pClient->m_pCountryFlags->Num(), 6, OldSelected, s_ScrollValue);

			for(int i = 0; i < m_pClient->m_pCountryFlags->Num(); ++i)
			{
				const CCountryFlags::CCountryFlag *pEntry = m_pClient->m_pCountryFlags->GetByIndex(i);
				if(pEntry->m_CountryCode == ActSelection)
					OldSelected = i;

				CListboxItem Item = UiDoListboxNextItem(&pEntry->m_CountryCode, OldSelected == i);
				if(Item.m_Visible)
				{
					CUIRect Label;
					Item.m_Rect.Margin(5.0f, &Item.m_Rect);
					Item.m_Rect.HSplitBottom(10.0f, &Item.m_Rect, &Label);
					float OldWidth = Item.m_Rect.w;
					Item.m_Rect.w = Item.m_Rect.h*2;
					Item.m_Rect.x += (OldWidth-Item.m_Rect.w)/ 2.0f;
					ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
					m_pClient->m_pCountryFlags->Render(pEntry->m_CountryCode, &Color, Item.m_Rect.x, Item.m_Rect.y, Item.m_Rect.w, Item.m_Rect.h);
					UI()->DoLabel(&Label, pEntry->m_aCountryCodeString, 10.0f, 0);
				}
			}

			const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
			if(OldSelected != NewSelected)
				ActSelection = m_pClient->m_pCountryFlags->GetByIndex(NewSelected)->m_CountryCode;

			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, Localize("Ok"), 0, &Part) || m_EnterPressed)
			{
				g_Config.m_BrFilterCountryIndex = ActSelection;
				Client()->ServerBrowserUpdate();
				m_Popup = POPUP_NONE;
			}

			if(m_EscapePressed)
			{
				ActSelection = g_Config.m_BrFilterCountryIndex;
				m_Popup = POPUP_NONE;
			}
		}
		else if(m_Popup == POPUP_DELETE_DEMO)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&No, &Yes);

			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// delete demo
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					char aBuf[512];
					str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					if(Storage()->RemoveFile(aBuf, m_lDemos[m_DemolistSelectedIndex].m_StorageType))
					{
						DemolistPopulate();
						DemolistOnUpdate(false);
					}
					else
						PopupMessage(Localize("Error"), Localize("Unable to delete the demo"), Localize("Ok"));
				}
			}
		}
		else if(m_Popup == POPUP_RENAME_DEMO)
		{
			CUIRect Label, TextBox, Ok, Abort;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&Abort, &Ok);

			Ok.VMargin(20.0f, &Ok);
			Abort.VMargin(20.0f, &Abort);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonOk = 0;
			if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// rename demo
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					char aBufOld[512];
					str_format(aBufOld, sizeof(aBufOld), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					int Length = str_length(m_aCurrentDemoFile);
					char aBufNew[512];
					if(Length <= 4 || m_aCurrentDemoFile[Length-5] != '.' || str_comp_nocase(m_aCurrentDemoFile+Length-4, "demo"))
						str_format(aBufNew, sizeof(aBufNew), "%s/%s.demo", m_aCurrentDemoFolder, m_aCurrentDemoFile);
					else
						str_format(aBufNew, sizeof(aBufNew), "%s/%s", m_aCurrentDemoFolder, m_aCurrentDemoFile);
					if(Storage()->RenameFile(aBufOld, aBufNew, m_lDemos[m_DemolistSelectedIndex].m_StorageType))
					{
						DemolistPopulate();
						DemolistOnUpdate(false);
					}
					else
						PopupMessage(Localize("Error"), Localize("Unable to rename the demo"), Localize("Ok"));
				}
			}

			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(120.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("New name:"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&Offset, &TextBox, m_aCurrentDemoFile, sizeof(m_aCurrentDemoFile), 12.0f, &Offset);
		}
#if defined(CONF_VIDEORECORDER)
		else if(m_Popup == POPUP_RENDER_DEMO)
		{
			CUIRect Label, TextBox, Ok, Abort, IncSpeed, DecSpeed, Button;

			Box.HSplitBottom(20.f, &Box, &Part);
#if defined(__ANDROID__)
			Box.HSplitBottom(60.f, &Box, &Part);
#else
			Box.HSplitBottom(24.f, &Box, &Part);
#endif
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&Abort, &Ok);

			Ok.VMargin(20.0f, &Ok);
			Abort.VMargin(20.0f, &Abort);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonOk = 0;
			if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// name video
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					char aBufOld[512];
					str_format(aBufOld, sizeof(aBufOld), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					int Length = str_length(m_aCurrentDemoFile);
					char aBufNew[512];
					if(Length <= 3 || m_aCurrentDemoFile[Length-4] != '.' || str_comp_nocase(m_aCurrentDemoFile+Length-3, "mp4"))
						str_format(aBufNew, sizeof(aBufNew), "%s.mp4", m_aCurrentDemoFile);
					else
						str_format(aBufNew, sizeof(aBufNew), "%s", m_aCurrentDemoFile);
					char aWholePath[1024];
					// store new video filename to origin buffer
					str_copy(m_aCurrentDemoFile, aBufNew, sizeof(m_aCurrentDemoFile));
					if(Storage()->FindFile(m_aCurrentDemoFile, "videos", IStorage::TYPE_ALL, aWholePath, sizeof(aWholePath)))
					{
						PopupMessage(Localize("Error"), Localize("Destination file already exist"), Localize("Ok"));
						m_Popup = POPUP_REPLACE_VIDEO;
					}
					else
					{
						const char *pError = Client()->DemoPlayer_Render(aBufOld, m_lDemos[m_DemolistSelectedIndex].m_StorageType, m_aCurrentDemoFile, m_Speed);
						m_Speed = 4;
						//Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "demo_render_path", aWholePath);
						if(pError)
							PopupMessage(Localize("Error"), str_comp(pError, "error loading demo") ? pError : Localize("Error loading demo"), Localize("Ok"));
					}
				}
			}
			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(20.f, &Box, &Part);
			Part.VSplitLeft(60.0f, 0, &Part);
			Part.VSplitLeft(60.0f, 0, &Label);
			Part.VSplitMid(&IncSpeed, &DecSpeed);

			IncSpeed.VMargin(20.0f, &IncSpeed);
			DecSpeed.VMargin(20.0f, &DecSpeed);

			Part.VSplitLeft(20.0f, &Button, &Part);
			bool IncDemoSpeed = false, DecDemoSpeed = false;
			// slowdown
			Part.VSplitLeft(5.0f, 0, &Part);
			Part.VSplitLeft(Button.h, &Button, &Part);
			static int s_SlowDownButton = 0;
			if(DoButton_Sprite(&s_SlowDownButton, IMAGE_DEMOBUTTONS, SPRITE_DEMOBUTTON_SLOWER, 0, &Button, CUI::CORNER_ALL))
				DecDemoSpeed = true;

			// fastforward
			Part.VSplitLeft(5.0f, 0, &Part);
			Part.VSplitLeft(Button.h, &Button, &Part);
			static int s_FastForwardButton = 0;
			if(DoButton_Sprite(&s_FastForwardButton, IMAGE_DEMOBUTTONS, SPRITE_DEMOBUTTON_FASTER, 0, &Button, CUI::CORNER_ALL))
				IncDemoSpeed = true;

			// speed meter
			Part.VSplitLeft(15.0f, 0, &Part);
			char aBuffer[64];
			str_format(aBuffer, sizeof(aBuffer), "Speed: ×%g", g_aSpeeds[m_Speed]);
			//str_format(aBuffer, sizeof(aBuffer), "Speed: ×%g", Speed);
			UI()->DoLabel(&Part, aBuffer, Button.h*0.7f, -1);

			if(IncDemoSpeed)
				m_Speed = clamp(m_Speed + 1, 0, (int)(sizeof(g_aSpeeds)/sizeof(g_aSpeeds[0])-1));
			else if(DecDemoSpeed)
				m_Speed = clamp(m_Speed - 1, 0, (int)(sizeof(g_aSpeeds)/sizeof(g_aSpeeds[0])-1));

			Part.VSplitLeft(100.0f, 0, &Part);
			Part.VSplitLeft(Button.h, &Button, &Part);
			if(DoButton_CheckBox(&g_Config.m_ClVideoShowhud, Localize("Show ingame HUD"), g_Config.m_ClVideoShowhud, &Button))
				g_Config.m_ClVideoShowhud ^= 1;
			Part.VSplitLeft(150.0f, 0, &Part);
			Part.VSplitLeft(Button.h, &Button, &Part);
			if(DoButton_CheckBox(&g_Config.m_ClVideoShowChat, Localize("Show chat"), g_Config.m_ClVideoShowChat, &Button))
				g_Config.m_ClVideoShowChat ^= 1;
			Part.VSplitLeft(150.0f, 0, &Part);
			Part.VSplitLeft(Button.h, &Button, &Part);
			if(DoButton_CheckBox(&g_Config.m_ClVideoSndEnable, Localize("Use sounds"), g_Config.m_ClVideoSndEnable, &Button))
				g_Config.m_ClVideoSndEnable ^= 1;

			Box.HSplitBottom(20.f, &Box, &Part);
#if defined(__ANDROID__)
			Box.HSplitBottom(60.f, &Box, &Part);
#else
			Box.HSplitBottom(24.f, &Box, &Part);
#endif

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(120.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Video name:"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&Offset, &TextBox, m_aCurrentDemoFile, sizeof(m_aCurrentDemoFile), 12.0f, &Offset);
		}
		else if(m_Popup == POPUP_REPLACE_VIDEO)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
#if defined(__ANDROID__)
			Box.HSplitBottom(60.f, &Box, &Part);
#else
			Box.HSplitBottom(24.f, &Box, &Part);
#endif
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&No, &Yes);

			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_RENDER_DEMO;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// render video
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
				const char *pError = Client()->DemoPlayer_Render(aBuf, m_lDemos[m_DemolistSelectedIndex].m_StorageType, m_aCurrentDemoFile, m_Speed);
				m_Speed = 4;
				if(pError)
					PopupMessage(Localize("Error"), str_comp(pError, "error loading demo") ? pError : Localize("Error loading demo"), Localize("Ok"));
			}
		}
#endif
		else if(m_Popup == POPUP_REMOVE_FRIEND)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&No, &Yes);

			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// remove friend
				if(m_FriendlistSelectedIndex >= 0)
				{
					m_pClient->Friends()->RemoveFriend(m_lFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aName,
						m_lFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aClan);
					FriendlistOnUpdate();
					Client()->ServerBrowserUpdate();
				}
			}
		}
		else if(m_Popup == POPUP_FIRST_LAUNCH)
		{
			CUIRect Label, TextBox;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			static int s_EnterButton = 0;
			if(DoButton_Menu(&s_EnterButton, Localize("Enter"), 0, &Part) || m_EnterPressed)
			{
				Client()->RequestDDNetInfo();
				m_Popup = POPUP_NONE;
			}

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(30.0f, 0, &Part);
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%s\n(%s)",
				Localize("Show DDNet map finishes in server browser"),
				Localize("transmits your player name to info.ddnet.tw"));

			if(DoButton_CheckBox(&g_Config.m_BrIndicateFinished, aBuf, g_Config.m_BrIndicateFinished, &Part))
				g_Config.m_BrIndicateFinished ^= 1;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(100.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Nickname"), 16.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&g_Config.m_PlayerName, &TextBox, g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName), 12.0f, &Offset);
		}
		else
		{
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || m_EscapePressed || m_EnterPressed)
			{
				if(m_Popup == POPUP_DISCONNECTED && Client()->m_ReconnectTime > 0)
					Client()->m_ReconnectTime = 0;
				m_Popup = POPUP_NONE;
			}
		}

		if(m_Popup == POPUP_NONE)
			UI()->SetActiveItem(0);
	}
	return 0;
}


void CMenus::SetActive(bool Active)
{
	if(Active != m_MenuActive)
		Input()->SetIMEState(Active);
	m_MenuActive = Active;
	if(!m_MenuActive)
	{
		if(m_NeedSendinfo)
		{
			m_pClient->SendInfo(false);
			m_NeedSendinfo = false;
		}

		if(m_NeedSendDummyinfo)
		{
			m_pClient->SendDummyInfo(false);
			m_NeedSendDummyinfo = false;
		}

		if(Client()->State() == IClient::STATE_ONLINE)
		{
			m_pClient->OnRelease();
		}
	}
	else if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		m_pClient->OnRelease();
	}
}

void CMenus::OnReset()
{
}

bool CMenus::OnMouseMove(float x, float y)
{
	m_LastInput = time_get();

	if(!m_MenuActive)
		return false;

	UI()->ConvertMouseMove(&x, &y);
	if(m_MouseSlow)
	{
		m_MousePos.x += x * 0.05f;
		m_MousePos.y += y * 0.05f;
	}
	else
	{
		m_MousePos.x += x;
		m_MousePos.y += y;
	}
	m_MousePos.x = clamp(m_MousePos.x, 0.f, (float)Graphics()->ScreenWidth());
	m_MousePos.y = clamp(m_MousePos.y, 0.f, (float)Graphics()->ScreenHeight());

	return true;
}

bool CMenus::OnInput(IInput::CEvent e)
{
	m_LastInput = time_get();

	// special handle esc and enter for popup purposes
	if(e.m_Flags&IInput::FLAG_PRESS)
	{
		if(e.m_Key == KEY_ESCAPE)
		{
			m_EscapePressed = true;
			if(m_Popup == POPUP_NONE)
				SetActive(!IsActive());
			return true;
		}
	}

	if(IsActive())
	{
		if(e.m_Flags&IInput::FLAG_PRESS)
		{
			// special for popups
			if(e.m_Key == KEY_RETURN || e.m_Key == KEY_KP_ENTER)
				m_EnterPressed = true;
			else if(e.m_Key == KEY_DELETE)
				m_DeletePressed = true;
		}

		if(m_NumInputEvents < MAX_INPUTEVENTS)
			m_aInputEvents[m_NumInputEvents++] = e;
		return true;
	}
	return false;
}

void CMenus::OnStateChange(int NewState, int OldState)
{
	// reset active item
	UI()->SetActiveItem(0);

	if(NewState == IClient::STATE_OFFLINE)
	{
		if(OldState >= IClient::STATE_ONLINE && NewState < IClient::STATE_QUITING)
			m_pClient->m_pSounds->Play(CSounds::CHN_MUSIC, SOUND_MENU, 1.0f);
		m_Popup = POPUP_NONE;
		if(Client()->ErrorString() && Client()->ErrorString()[0] != 0)
		{
			if(str_find(Client()->ErrorString(), "password"))
			{
				m_Popup = POPUP_PASSWORD;
				UI()->SetHotItem(&g_Config.m_Password);
				UI()->SetActiveItem(&g_Config.m_Password);
			}
			else
				m_Popup = POPUP_DISCONNECTED;
		}
	}
	else if(NewState == IClient::STATE_LOADING)
	{
		m_Popup = POPUP_CONNECTING;
		m_DownloadLastCheckTime = time_get();
		m_DownloadLastCheckSize = 0;
		m_DownloadSpeed = 0.0f;
	}
	else if(NewState == IClient::STATE_CONNECTING)
		m_Popup = POPUP_CONNECTING;
	else if(NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
	{
		m_Popup = POPUP_NONE;
		SetActive(false);
	}
}

extern "C" void font_debug_render();

void CMenus::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		SetActive(true);

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		CUIRect Screen = *UI()->Screen();
		Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
		RenderDemoPlayer(Screen);
	}

	if(Client()->State() == IClient::STATE_ONLINE && m_pClient->m_ServerMode == m_pClient->SERVERMODE_PUREMOD)
	{
		Client()->Disconnect();
		SetActive(true);
		m_Popup = POPUP_PURE;
	}

	if(!IsActive())
	{
		m_EscapePressed = false;
		m_EnterPressed = false;
		m_DeletePressed = false;
		m_NumInputEvents = 0;
		return;
	}

	// update colors
	ms_GuiColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_UiColor, true));

	ms_ColorTabbarInactiveOutgame = ColorRGBA(0,0,0,0.25f);
	ms_ColorTabbarActiveOutgame = ColorRGBA(0,0,0,0.5f);

	float ColorIngameScaleI = 0.5f;
	float ColorIngameAcaleA = 0.2f;
	ms_ColorTabbarInactiveIngame = ColorRGBA(
		ms_GuiColor.r*ColorIngameScaleI,
		ms_GuiColor.g*ColorIngameScaleI,
		ms_GuiColor.b*ColorIngameScaleI,
		ms_GuiColor.a*0.8f);

	ms_ColorTabbarActiveIngame = ColorRGBA(
		ms_GuiColor.r*ColorIngameAcaleA,
		ms_GuiColor.g*ColorIngameAcaleA,
		ms_GuiColor.b*ColorIngameAcaleA,
		ms_GuiColor.a);

	// update the ui
	CUIRect *pScreen = UI()->Screen();
	float mx = (m_MousePos.x/(float)Graphics()->ScreenWidth())*pScreen->w;
	float my = (m_MousePos.y/(float)Graphics()->ScreenHeight())*pScreen->h;

	int Buttons = 0;
	if(m_UseMouseButtons)
	{
		if(Input()->KeyIsPressed(KEY_MOUSE_1)) Buttons |= 1;
		if(Input()->KeyIsPressed(KEY_MOUSE_2)) Buttons |= 2;
		if(Input()->KeyIsPressed(KEY_MOUSE_3)) Buttons |= 4;
	}

	UI()->Update(mx,my,mx*3.0f,my*3.0f,Buttons);

	// render
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
		Render();

	// render cursor
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1,1,1,1);
	IGraphics::CQuadItem QuadItem(mx, my, 24, 24);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	// render debug information
	if(g_Config.m_Debug)
	{
		CUIRect Screen = *UI()->Screen();
		Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%p %p %p", UI()->HotItem(), UI()->ActiveItem(), UI()->LastActiveItem());
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 10, 10, 10, TEXTFLAG_RENDER);
		TextRender()->TextEx(&Cursor, aBuf, -1);
	}

	m_EscapePressed = false;
	m_EnterPressed = false;
	m_DeletePressed = false;
	m_NumInputEvents = 0;
}

void CMenus::RenderBackground()
{
	float sw = 300*Graphics()->ScreenAspect();
	float sh = 300;
	Graphics()->MapScreen(0, 0, sw, sh);

	// render background color
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
		ColorRGBA Bottom(ms_GuiColor.r, ms_GuiColor.g, ms_GuiColor.b, 1.0f);
		ColorRGBA Top(ms_GuiColor.r, ms_GuiColor.g, ms_GuiColor.b, 1.0f);
		IGraphics::CColorVertex Array[4] = {
			IGraphics::CColorVertex(0, Top.r, Top.g, Top.b, Top.a),
			IGraphics::CColorVertex(1, Top.r, Top.g, Top.b, Top.a),
			IGraphics::CColorVertex(2, Bottom.r, Bottom.g, Bottom.b, Bottom.a),
			IGraphics::CColorVertex(3, Bottom.r, Bottom.g, Bottom.b, Bottom.a)};
		Graphics()->SetColorVertex(Array, 4);
		IGraphics::CQuadItem QuadItem(0, 0, sw, sh);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	// render the tiles
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
		float Size = 15.0f;
		float OffsetTime = fmod(LocalTime()*0.15f, 2.0f);
		for(int y = -2; y < (int)(sw/Size); y++)
			for(int x = -2; x < (int)(sh/Size); x++)
			{
				Graphics()->SetColor(0,0,0,0.045f);
				IGraphics::CQuadItem QuadItem((x-OffsetTime)*Size*2+(y&1)*Size, (y+OffsetTime)*Size, Size, Size);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
			}
	Graphics()->QuadsEnd();

	// render border fade
	Graphics()->TextureSet(m_TextureBlob);
	Graphics()->QuadsBegin();
		Graphics()->SetColor(1,1,1,1);
		QuadItem = IGraphics::CQuadItem(-100, -100, sw+200, sh+200);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	// restore screen
	{CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);}
}

int CMenus::DoButton_CheckBox_DontCare(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	switch(Checked)
	{
	case 0:
		return DoButton_CheckBox_Common(pID, pText, "", pRect);
	case 1:
		return DoButton_CheckBox_Common(pID, pText, "X", pRect);
	case 2:
		return DoButton_CheckBox_Common(pID, pText, "O", pRect);
	default:
		return DoButton_CheckBox_Common(pID, pText, "", pRect);
	}
}

void CMenus::RenderUpdating(const char *pCaption, int current, int total)
{
	// make sure that we don't render for each little thing we load
	// because that will slow down loading if we have vsync
	static int64 LastLoadRender = 0;
	if(time_get()-LastLoadRender < time_freq()/60)
		return;
	LastLoadRender = time_get();

	// need up date this here to get correct
	ms_GuiColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_UiColor, true));

	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	Graphics()->BlendNormal();
	RenderBackground();

	float w = 700;
	float h = 200;
	float x = Screen.w/2-w/2;
	float y = Screen.h/2-h/2;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0,0,0,0.50f);
	RenderTools()->DrawRoundRect(0, y, Screen.w, h, 0.0f);
	Graphics()->QuadsEnd();

	CUIRect r;
	r.x = x;
	r.y = y+20;
	r.w = w;
	r.h = h;
	UI()->DoLabel(&r, Localize(pCaption), 32.0f, 0, -1);

	if(total>0)
	{
		float Percent = current/(float)total;
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.15f,0.15f,0.15f,0.75f);
		RenderTools()->DrawRoundRect(x+40, y+h-75, w-80, 30, 5.0f);
		Graphics()->SetColor(1,1,1,0.75f);
		RenderTools()->DrawRoundRect(x+45, y+h-70, (w-85)*Percent, 20, 5.0f);
		Graphics()->QuadsEnd();
	}

	Graphics()->Swap();
}
