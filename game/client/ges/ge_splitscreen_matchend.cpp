///////////// Copyright (c) 2026, GoldenEye: Source split screen. All rights reserved. /////////////
//
//   ge_splitscreen_matchend.cpp
//
//   After a local split screen match ends, the results show for 5 seconds, then a
//   bar at the bottom of the screen offers "Play Again" (the same map and settings)
//   or "Return to Menu" (the Split Screen menu). Every player's controller can use it
//   (d-pad / left stick to choose, A / right trigger / START to pick), and player 1's
//   keyboard (arrow keys, Enter). Meanwhile the server keeps the match on its results
//   instead of moving to the next map (ges_ss_matchend_hold, ge_gameplay.cpp).
//
///////////////////////////////////////////////////////////////////////////////
#include "cbase.h"
#include "ge_splitscreen_client.h"
#include "ge_splitscreen_gamepad.h"
#include "GameEventListener.h"
#include "iclientmode.h"
#include "inetchannelinfo.h"
#include "inputsystem/iinputsystem.h"
#include "ienginevgui.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui_controls/Panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar ges_ss_matchend_prompt( "ges_ss_matchend_prompt", "1", FCVAR_ARCHIVE, "Split screen: after a match, offer Play Again / Return to Menu instead of moving on to the next map." );
static ConVar ges_ss_matchend_delay( "ges_ss_matchend_delay", "5", FCVAR_ARCHIVE, "Split screen: seconds the match results show before Play Again / Return to Menu appears.", true, 0, true, 60 );

enum
{
	GESS_END_PLAYAGAIN = 0,
	GESS_END_MENU,
	GESS_END_COUNT
};
static const char *s_pszEndLabels[GESS_END_COUNT] = { "Play Again", "Return to Menu" };

static float	s_flMatchOverTime = 0;		// gpGlobals->realtime the match ended, 0 = playing
static int		s_iEndSel = GESS_END_PLAYAGAIN;
static bool		s_bEndChosen = false;
static bool		s_bPadStickHeld[GES_SS_MAX_PADS];
static bool		s_bKeyHeld[BUTTON_CODE_LAST];

// Our own split screen match, on our own server
static bool GESS_IsLocalSplitMatch()
{
	if ( !GESplitScreenClient()->IsSplitMatch() )
		return false;
	INetChannelInfo *pNet = engine->GetNetChannelInfo();
	return pNet && pNet->IsLoopback();
}

static bool GESS_MatchEndPromptShown()
{
	return s_flMatchOverTime > 0 && !s_bEndChosen && engine->IsInGame() &&
		gpGlobals->realtime >= s_flMatchOverTime + ges_ss_matchend_delay.GetFloat();
}

//-----------------------------------------------------------------------------
// The match is over: GoldenEye's final "round_end" (the one with the match report)
//-----------------------------------------------------------------------------
class CGESSMatchEndListener : public CGameEventListener
{
public:
	CGESSMatchEndListener() : m_bListening( false ) {}

	void Listen()
	{
		if ( m_bListening || !gameeventmanager )
			return;
		ListenForGameEvent( "round_end" );
		m_bListening = true;
	}

	virtual void FireGameEvent( IGameEvent *event )
	{
		if ( Q_stricmp( event->GetName(), "round_end" ) || !event->GetBool( "isfinal" ) )
			return;
		// Player 1 chose Exit game: the results show, then straight back to the menu
		if ( !ges_ss_matchend_prompt.GetBool() || !GESS_IsLocalSplitMatch() || GESplitScreenClient()->IsExitingToMenu() )
			return;

		s_flMatchOverTime = gpGlobals->realtime;
		s_iEndSel = GESS_END_PLAYAGAIN;
		s_bEndChosen = false;
	}

private:
	bool m_bListening;
};
static CGESSMatchEndListener s_MatchEndListener;

//-----------------------------------------------------------------------------
// The bar at the bottom of the screen, drawn like the START menu's buttons. A popup
// of its own so it sits over GoldenEye's match report (also a popup). It never
// takes the keyboard or mouse: input is read in GESS_MatchEnd_Update.
//-----------------------------------------------------------------------------
class CGESSMatchEndPanel : public Panel
{
	DECLARE_CLASS_SIMPLE( CGESSMatchEndPanel, Panel );

public:
	CGESSMatchEndPanel( VPANEL parent ) : BaseClass( NULL, "GESSMatchEnd" )
	{
		SetParent( parent );
		SetPaintBackgroundEnabled( false );
		SetPaintBorderEnabled( false );
		SetKeyBoardInputEnabled( false );
		SetMouseInputEnabled( false );
		MakePopup( false, false );
		SetKeyBoardInputEnabled( false );
		SetMouseInputEnabled( false );
		SetVisible( false );
		m_hFontSmall = m_hFontLarge = 0;
		m_iFontScreenTall = 0;
		m_flNextFront = 0;
	}

	void UpdateShown( bool bShow )
	{
		if ( !bShow )
		{
			if ( IsVisible() )
				SetVisible( false );
			return;
		}

		int w, h;
		surface()->GetScreenSize( w, h );
		UpdateFonts( h );

		// A box like the START menu's, across the bottom of the screen
		const int tallL = surface()->GetFontTall( m_hFontLarge );
		const int tallS = surface()->GetFontTall( m_hFontSmall );
		const int boxH = 6 + tallL + 10 + ( tallL + 8 ) + 10 + tallS + 8;
		const int boxW = Min( w - 20, Max( w * 11 / 20, TextWide( m_hFontLarge, "Return to Menu" ) * 3 ) );
		const int margin = Max( 8, h / 30 );
		SetBounds( ( w - boxW ) / 2, h - margin - boxH, boxW, boxH );

		if ( !IsVisible() )
		{
			SetVisible( true );
			m_flNextFront = 0;
		}
		// Stay above the match report
		if ( gpGlobals->realtime >= m_flNextFront )
		{
			MoveToFront();
			m_flNextFront = gpGlobals->realtime + 0.5f;
		}
	}

	virtual void Paint()
	{
		int bw, bh;
		GetSize( bw, bh );
		const int tallL = surface()->GetFontTall( m_hFontLarge );
		const int tallS = surface()->GetFontTall( m_hFontSmall );
		const int cx = bw / 2;

		// Dark background with the title on top, help underneath (the START menu's look)
		surface()->DrawSetColor( Color( 10, 10, 10, 225 ) );
		surface()->DrawFilledRect( 0, 0, bw, bh );

		int y = 6;
		DrawText( m_hFontLarge, cx, y, Color( 230, 200, 80, 255 ), "Match over", true );
		y += tallL + 10;

		// Two buttons side by side
		const int btnH = tallL + 8;
		const int gap = Max( 12, bw / 30 );
		const int btnW = Min( ( bw - gap * 3 ) / 2, Max( TextWide( m_hFontLarge, s_pszEndLabels[0] ), TextWide( m_hFontLarge, s_pszEndLabels[1] ) ) + 40 );
		const int left = cx - btnW - gap / 2;
		for ( int i = 0; i < GESS_END_COUNT; i++ )
		{
			const int bx = left + i * ( btnW + gap );
			const bool bSel = s_iEndSel == i;
			surface()->DrawSetColor( bSel ? Color( 150, 125, 50, 255 ) : Color( 40, 40, 40, 255 ) );
			surface()->DrawFilledRect( bx, y, bx + btnW, y + btnH );
			surface()->DrawSetColor( bSel ? Color( 255, 230, 120, 255 ) : Color( 90, 90, 90, 255 ) );
			surface()->DrawOutlinedRect( bx, y, bx + btnW, y + btnH );
			DrawText( m_hFontLarge, bx + btnW / 2, y + 4, Color( 255, 255, 255, 255 ), s_pszEndLabels[i], true );
		}
		y += btnH + 10;

		DrawText( m_hFontSmall, cx, y, Color( 200, 200, 200, 230 ), "Left / Right: choose    A / START: select", true );
	}

private:
	void UpdateFonts( int h )
	{
		if ( h <= 0 || h == m_iFontScreenTall )
			return;
		m_iFontScreenTall = h;

		// The split screen HUD's fonts (ge_splitscreen_hud.cpp)
		const int flags = ISurface::FONTFLAG_ANTIALIAS | ISurface::FONTFLAG_DROPSHADOW;
		HFont small = surface()->CreateFont();
		if ( surface()->SetFontGlyphSet( small, "Verdana", clamp( h / 50, 12, 64 ), 700, 0, 0, flags ) )
			m_hFontSmall = small;
		HFont large = surface()->CreateFont();
		if ( surface()->SetFontGlyphSet( large, "Verdana", clamp( h / 30, 16, 96 ), 700, 0, 0, flags ) )
			m_hFontLarge = large;
	}

	int TextWide( HFont font, const char *text )
	{
		wchar_t wbuf[128];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
		int tw, th;
		surface()->GetTextSize( font, wbuf, tw, th );
		return tw;
	}

	void DrawText( HFont font, int x, int y, Color col, const char *text, bool bCenter )
	{
		wchar_t wbuf[128];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
		if ( bCenter )
		{
			int tw, th;
			surface()->GetTextSize( font, wbuf, tw, th );
			x -= tw / 2;
		}
		surface()->DrawSetTextFont( font );
		surface()->DrawSetTextColor( col );
		surface()->DrawSetTextPos( x, y );
		surface()->DrawPrintText( wbuf, wcslen( wbuf ) );
	}

	HFont	m_hFontSmall;
	HFont	m_hFontLarge;
	int		m_iFontScreenTall;
	float	m_flNextFront;
};

static DHANDLE<CGESSMatchEndPanel> s_hMatchEndPanel;

//-----------------------------------------------------------------------------
// The choices
//-----------------------------------------------------------------------------
static void GESS_MatchEnd_Choose( int choice )
{
	s_bEndChosen = true;
	if ( s_hMatchEndPanel.Get() )
		s_hMatchEndPanel->UpdateShown( false );

	if ( choice == GESS_END_PLAYAGAIN )
	{
		// The same map again, everyone still in, the same settings
		char szMap[MAX_PATH];
		V_FileBase( engine->GetLevelName(), szMap, sizeof( szMap ) );
		Msg( "Split screen: play again (%s)\n", szMap );
		// The engine's own changelevel: GoldenEye's checks the map with engine->IsMapValid,
		// which wrongly says no to this install's maps
		engine->ClientCmd_Unrestricted( VarArgs( "__real_changelevel %s\n", szMap ) );
	}
	else
	{
		Msg( "Split screen: back to the menu\n" );
		engine->ClientCmd_Unrestricted( "disconnect\n" );
		engine->ClientCmd_Unrestricted( "ges_splitscreen_match\n" );
	}
}

static bool GESS_KeyPressed( ButtonCode_t code )
{
	const bool bDown = inputsystem && inputsystem->IsButtonDown( code );
	const bool bPressed = bDown && !s_bKeyHeld[code];
	s_bKeyHeld[code] = bDown;
	return bPressed;
}

//-----------------------------------------------------------------------------
// Per frame, from CGESplitScreenClient::Update before the players' own input.
// Returns true while the prompt is up (the controllers' presses are used here).
//-----------------------------------------------------------------------------
bool GESS_MatchEnd_Update()
{
	s_MatchEndListener.Listen();

	// Keep a finished split screen match on its results until a choice is made
	// (the server's cvar, found each time: server.dll loads with the first map)
	// (Kept on after a choice: the level change is on its way)
	const bool bSplitMatch = engine->IsInGame() && GESS_IsLocalSplitMatch();
	ConVar *pHold = g_pCVar ? g_pCVar->FindVar( "ges_ss_matchend_hold" ) : NULL;
	if ( pHold )
	{
		const bool bHold = ges_ss_matchend_prompt.GetBool() && bSplitMatch && !GESplitScreenClient()->IsExitingToMenu();
		if ( pHold->GetBool() != bHold )
			pHold->SetValue( bHold ? 1 : 0 );
	}
	// The server knows it's a split screen match (one match report, team modes with one player a team)
	ConVar *pSplit = g_pCVar ? g_pCVar->FindVar( "ges_ss_splitmatch" ) : NULL;
	if ( pSplit && pSplit->GetBool() != bSplitMatch )
		pSplit->SetValue( bSplitMatch ? 1 : 0 );

	// Left the game or a new map loaded
	if ( !engine->IsInGame() )
		s_flMatchOverTime = 0;

	const bool bShow = GESS_MatchEndPromptShown() && !enginevgui->IsGameUIVisible();
	if ( bShow && !s_hMatchEndPanel.Get() && g_pClientMode && g_pClientMode->GetViewport() )
		s_hMatchEndPanel = new CGESSMatchEndPanel( g_pClientMode->GetViewport()->GetVPanel() );
	if ( s_hMatchEndPanel.Get() )
		s_hMatchEndPanel->UpdateShown( bShow );

	// Keyboard edges are tracked all the time, so a key held from the match doesn't count
	const bool bKeyLeft = GESS_KeyPressed( KEY_LEFT ) | GESS_KeyPressed( KEY_UP ) | GESS_KeyPressed( KEY_A ) | GESS_KeyPressed( KEY_W );
	const bool bKeyRight = GESS_KeyPressed( KEY_RIGHT ) | GESS_KeyPressed( KEY_DOWN ) | GESS_KeyPressed( KEY_D ) | GESS_KeyPressed( KEY_S );
	const bool bKeyPick = GESS_KeyPressed( KEY_ENTER ) | GESS_KeyPressed( KEY_PAD_ENTER ) | GESS_KeyPressed( KEY_SPACE );

	if ( !bShow )
	{
		for ( int pad = 0; pad < GES_SS_MAX_PADS; pad++ )
			s_bPadStickHeld[pad] = true;	// the stick must come back to the middle first
		return false;
	}

	int dir = bKeyLeft ? -1 : ( bKeyRight ? 1 : 0 );
	bool bPick = bKeyPick;

	for ( int pad = 0; pad < GES_SS_MAX_PADS; pad++ )
	{
		const GESSPadState_t &p = GESSGamepads()->GetPad( pad );
		if ( !p.connected )
			continue;

		if ( p.WasPressed( GESS_PAD_DPAD_LEFT ) || p.WasPressed( GESS_PAD_DPAD_UP ) )
			dir = -1;
		else if ( p.WasPressed( GESS_PAD_DPAD_RIGHT ) || p.WasPressed( GESS_PAD_DPAD_DOWN ) )
			dir = 1;

		// The left stick: one step each time it's pushed left or right
		const bool bStick = fabsf( p.lx ) > 0.6f;
		if ( bStick && !s_bPadStickHeld[pad] )
			dir = p.lx > 0 ? 1 : -1;
		s_bPadStickHeld[pad] = fabsf( p.lx ) > 0.3f;

		if ( p.WasPressed( GESS_PAD_A ) || p.WasPressed( GESS_PAD_RT ) || p.WasPressed( GESS_PAD_START ) )
			bPick = true;

		// The presses are ours: no START menu, jump or fire from them
		GESSGamepads()->ConsumeEdges( pad );
	}

	if ( dir )
		s_iEndSel = clamp( s_iEndSel + dir, 0, GESS_END_COUNT - 1 );
	if ( bPick )
		GESS_MatchEnd_Choose( s_iEndSel );

	return true;
}

void GESS_MatchEnd_Reset()
{
	s_flMatchOverTime = 0;
	s_bEndChosen = false;
	if ( s_hMatchEndPanel.Get() )
		s_hMatchEndPanel->UpdateShown( false );
}

CON_COMMAND( ges_ss_debug_matchend, "Split screen: show the Play Again / Return to Menu prompt now (test)" )
{
	s_flMatchOverTime = gpGlobals->realtime - ges_ss_matchend_delay.GetFloat();
	s_iEndSel = GESS_END_PLAYAGAIN;
	s_bEndChosen = false;
}
