///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_splitscreen_hud.cpp
//   Description :
//      Split screen HUD. While the screen is split the normal HUD is hidden
//      (it belongs to player 1 and would cover everyone) and this panel draws
//      a compact HUD inside each player's viewport, plus the join and profile
//      menus that players drive with their controllers.
//
////////////////////////////////////////////////////////////////////////////

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui/IInput.h"
#include "vgui_controls/Panel.h"

#include "c_ge_player.h"
#include "c_gemp_player.h"
#include "ge_splitscreen_client.h"
#include "ge_splitscreen_gamepad.h"
#include "ge_splitscreen_profiles.h"
#include "ge_character_data.h"
#include "c_playerresource.h"
#include "gemp_gamerules.h"
#include "c_team.h"
#include "c_ge_playerresource.h"
#include "ge_shareddefs.h"
#include "ge_utils.h"
#include "ge_weapon.h"
#include "weapons_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

void GESS_PaintRadar( int slot, C_BasePlayer *pViewer, int vx, int vy, int vw, int vh );

ConVar ges_ss_hud_hint( "ges_ss_hud_hint", "1", FCVAR_ARCHIVE, "Show 'press START to join' when controllers are connected." );

class CGESplitScreenHud : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGESplitScreenHud, vgui::Panel );

public:
	CGESplitScreenHud( const char *pElementName );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual bool ShouldDraw();
	virtual void OnThink();
	virtual void Paint();

private:
	void PaintSlot( int slot, const GESSViewport_t &vp );
	void PaintPlayerHud( int slot, C_BasePlayer *pPlayer, const GESSViewport_t &vp );
	void PaintMenu( int slot, const GESSViewport_t &vp );
	void PaintPadDisconnected( int slot, const GESSViewport_t &vp );
	void PaintObserverStatus( C_BasePlayer *pPlayer, const GESSViewport_t &vp );
	void PaintPregameSlot( int slot, const GESSViewport_t &vp );
	void PaintPregameHelp( int w, int h );
	void PaintScoreboard( const GESSViewport_t &vp, int highlightPlayer = -1 );
	// GoldenEye's own scoreboard and ammo counter, drawn in a part of the screen
	void PaintGEScoreboard( const GESSViewport_t &area, int highlightPlayer );
	void PaintGEAmmo( int slot, C_BasePlayer *pPlayer, const GESSViewport_t &vp );
	// Fonts at any size (the scheme's are sized for the whole screen), kept for reuse
	HFont GetScaledFont( const char *pszFace, int tall, int weight, int flags );
	struct ScaledFont_t
	{
		char	face[64];
		int		tall, weight, flags;
		HFont	font;
	};
	CUtlVector<ScaledFont_t> m_ScaledFonts;
	void PaintGameMenu( int slot, const GESSViewport_t &vp );
	// The START menu's Controller page and Calibrate Sticks screen (return the help line)
	const char *PaintControllerPage( int slot, const GESSViewport_t &area );
	const char *PaintCalibrate( int slot, const GESSViewport_t &area );
	// A row's highlight, and a slider (filled to frac) with its value at the right
	void PaintMenuRowBack( int x, int y, int w, int h, bool bSel );
	void PaintSlider( int slot, int row, int x, int y, int w, int h, float frac, const char *value, bool bSel );
	void PaintLogo( const GESSViewport_t &vp );
	void PaintJoinHint( const GESSViewport_t &vp, const char *line1, const char *line2 );
	// GoldenEye's own health and armour dials (the normal game's CGEHudLifebars),
	// either side of the crosshair, sized and placed for one player's view
	// With pAround (the START menu's leaderboard), they're drawn larger, wrapped around
	// the outside of that box instead
	void PaintLifeDials( C_GEPlayer *pGE, const GESSViewport_t &vp, float alpha, const GESSViewport_t *pAround = NULL );
	// How visible a player's health / armour bars are (0-1): they show when the player is
	// hit, healed or spawns, then fade, like the single player game
	float UpdateBarVisibility( int slot, C_BasePlayer *pPlayer, int health, int armor );

	struct BarState_t
	{
		int		player, health, armor;
		float	showUntil;
	};
	BarState_t m_Bars[GES_SS_MAX_PLAYERS];
	void DrawText( HFont font, int x, int y, Color col, const char *text, bool bCenter = false );
	int  TextWide( HFont font, const char *text );

	HFont m_hFontSmall;
	HFont m_hFontLarge;
	int m_iFontScreenTall;
	void UpdateFonts();
};

DECLARE_HUDELEMENT( CGESplitScreenHud );

CGESplitScreenHud::CGESplitScreenHud( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "GESplitScreenHud" )
{
	SetParent( g_pClientMode->GetViewport() );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	m_hFontSmall = 0;
	m_hFontLarge = 0;
	m_iFontScreenTall = 0;
	for ( int i = 0; i < GES_SS_MAX_PLAYERS; i++ )
	{
		m_Bars[i].player = -1;
		m_Bars[i].health = m_Bars[i].armor = -1;
		m_Bars[i].showUntil = 0;
	}
}

void CGESplitScreenHud::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_hFontSmall = pScheme->GetFont( "DefaultSmall", false );
	if ( !m_hFontSmall )
		m_hFontSmall = pScheme->GetFont( "Default", false );
	m_hFontLarge = pScheme->GetFont( "DefaultLarge", false );
	if ( !m_hFontLarge )
		m_hFontLarge = pScheme->GetFont( "Default", false );
	m_iFontScreenTall = 0;	// our own screen-sized fonts are made in UpdateFonts
	SetPaintBackgroundEnabled( false );
}

// Fonts sized from the screen height so the text reads the same at 720p, 1080p and
// 4K. The scheme's proportional fonts get too tall for the font cache at 4K and
// then draw nothing.
void CGESplitScreenHud::UpdateFonts()
{
	int w, h;
	surface()->GetScreenSize( w, h );
	if ( h <= 0 || h == m_iFontScreenTall )
		return;
	m_iFontScreenTall = h;

	const int flags = vgui::ISurface::FONTFLAG_ANTIALIAS | vgui::ISurface::FONTFLAG_DROPSHADOW;
	HFont small = surface()->CreateFont();
	if ( surface()->SetFontGlyphSet( small, "Verdana", clamp( h / 50, 12, 64 ), 700, 0, 0, flags ) )
		m_hFontSmall = small;
	HFont large = surface()->CreateFont();
	if ( surface()->SetFontGlyphSet( large, "Verdana", clamp( h / 30, 16, 96 ), 700, 0, 0, flags ) )
		m_hFontLarge = large;
}

bool CGESplitScreenHud::ShouldDraw()
{
	if ( !C_BasePlayer::GetLocalPlayer() )
		return false;

	CGESplitScreenClient *pSS = GESplitScreenClient();
	if ( pSS->IsSplitActive() )
		return true;

	// Not split: still show player 1's controller menu, and the join hint
	if ( pSS->GetSlotMenu( 0 ) != GESS_MENU_NONE || pSS->IsSlotPadDisconnected( 0 ) || pSS->IsPregame() )
		return true;

	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( pSS->ShouldSuppressGameMenus() && ( pLocal->IsObserver() || pLocal->GetTeamNumber() == TEAM_SPECTATOR ) )
		return true;

	return ges_ss_hud_hint.GetBool() && pSS->HasAnyPadConnected();
}

void CGESplitScreenHud::OnThink()
{
	int w, h;
	surface()->GetScreenSize( w, h );
	int x, y, cw, ch;
	GetBounds( x, y, cw, ch );
	if ( x != 0 || y != 0 || cw != w || ch != h )
		SetBounds( 0, 0, w, h );
	UpdateFonts();
}

void CGESplitScreenHud::DrawText( HFont font, int x, int y, Color col, const char *text, bool bCenter )
{
	wchar_t wsz[128];
	g_pVGuiLocalize->ConvertANSIToUnicode( text, wsz, sizeof( wsz ) );

	if ( bCenter )
	{
		int tw, th;
		surface()->GetTextSize( font, wsz, tw, th );
		x -= tw / 2;
	}

	surface()->DrawSetTextFont( font );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawPrintText( wsz, wcslen( wsz ) );
}

int CGESplitScreenHud::TextWide( HFont font, const char *text )
{
	wchar_t wsz[128];
	g_pVGuiLocalize->ConvertANSIToUnicode( text, wsz, sizeof( wsz ) );
	int tw, th;
	surface()->GetTextSize( font, wsz, tw, th );
	return tw;
}

ConVar ges_ss_hud_bars_hit( "ges_ss_hud_bars_hit", "1", FCVAR_ARCHIVE, "Split screen: 1 shows a player's health and armour only for a few seconds when they're hit, healed or spawn (like the single player game). 0 always shows them." );
ConVar ges_ss_hud_bars_time( "ges_ss_hud_bars_time", "2.8", FCVAR_ARCHIVE, "Split screen: seconds the health and armour stay on screen after a hit, including the last second where they fade out (the normal game: 1.8 + 1)." );

float CGESplitScreenHud::UpdateBarVisibility( int slot, C_BasePlayer *pPlayer, int health, int armor )
{
	if ( slot < 0 || slot >= GES_SS_MAX_PLAYERS )
		return 1.0f;

	BarState_t &bs = m_Bars[slot];
	const float now = gpGlobals->realtime;
	if ( bs.player != pPlayer->entindex() || bs.health != health || bs.armor != armor )
	{
		// Hit, healed, armour picked up or spawned (a new player counts as spawning)
		bs.player = pPlayer->entindex();
		bs.health = health;
		bs.armor = armor;
		bs.showUntil = now + ges_ss_hud_bars_time.GetFloat();
	}

	if ( !ges_ss_hud_bars_hit.GetBool() )
		return 1.0f;

	// Fade out over the last second, like the normal game's StatusBarFlash
	const float fade = 1.0f;
	return clamp( ( bs.showUntil - now ) / fade, 0.0f, 1.0f );
}

// The normal game's dial fill (ge_hudlifebars.cpp): the first 5/8 of the
// health or armour fill the small end of the dial
static float GESS_CalcDialPoints( float curr, float max )
{
	if ( max <= 0 )
		return 0;
	const float prop = curr / max;
	return ( prop < 0.625f ) ? ( prop * 8.0f / 11.0f ) : ( prop * 16.0f / 11.0f - 5.0f / 11.0f );
}

// The dial's sizes in the normal game (ge_hudlifebars.cpp, 480 tall units): one dial
// is 115 x 350, its inner edge 130 out from the centre, curved on a radius of 97 + 115
#define GESS_DIAL_W			115.0f
#define GESS_DIAL_H			350.0f
#define GESS_DIAL_RADIUS	( 97.0f + 115.0f )
// Around the leaderboard: the dials are this much taller than the green screen they
// wrap around, with this gap (in scoreboard units) between its edge and theirs
#define GESS_BOARD_DIAL_TALL	1.08f
#define GESS_BOARD_DIAL_GAP		6.0f
// The scoreboard's visible green screen, in its own units (see PaintGEScoreboard)
#define GESS_BOARD_VIS_W		422.0f
#define GESS_BOARD_VIS_H		262.0f

void CGESplitScreenHud::PaintLifeDials( C_GEPlayer *pGE, const GESSViewport_t &vp, float alpha, const GESSViewport_t *pAround )
{
	CHudTexture *pHealthFull = gHUD.GetIcon( "health_full" );
	CHudTexture *pHealthEmpty = gHUD.GetIcon( "health_empty" );
	CHudTexture *pArmorFull = gHUD.GetIcon( "armor_full" );
	CHudTexture *pArmorEmpty = gHUD.GetIcon( "armor_empty" );
	CHudTexture *pArmorDeny = gHUD.GetIcon( "armor_deny" );
	if ( !pHealthFull || !pHealthEmpty || !pArmorFull || !pArmorEmpty )
		return;

	// The normal game's sizes are for a 480 tall screen (proportional); this
	// player's screen is the view's height
	// Laid out for the view's own shape, like the normal game on a 16:9 screen:
	// in views wider than that (2 players: top / bottom halves) they keep their
	// size but sit as far out as on a 16:9 screen of that width; in narrower
	// views (5-8 players) they shrink to fit the width
	const float wide = 16.0f / 9.0f;
	const float aspect = vp.h > 0 ? (float) vp.w / (float) vp.h : wide;
	const float s = ( aspect >= wide ? vp.h : vp.w / wide ) / 480.0f;
	const float spread = Max( 1.0f, aspect / wide );
	int w = (int)( GESS_DIAL_W * s ), h = (int)( GESS_DIAL_H * s );
	int offset = (int)( 130 * s * spread ), radius = (int)( GESS_DIAL_RADIUS * s );
	int cx = vp.x + vp.w / 2, cy = vp.y + vp.h / 2;
	if ( pAround && pAround->h > 0 )
	{
		// Wrapped around the leaderboard: a little taller than it, just outside its
		// sides, so neither covers the other
		const float k = pAround->h * GESS_BOARD_DIAL_TALL / GESS_DIAL_H;
		const float gap = pAround->h * GESS_BOARD_DIAL_GAP / GESS_BOARD_VIS_H;
		w = (int)( GESS_DIAL_W * k );
		h = (int)( GESS_DIAL_H * k );
		radius = (int)( GESS_DIAL_RADIUS * k );
		offset = (int)( pAround->w / 2 + gap );
		cx = pAround->x + pAround->w / 2;
		cy = pAround->y + pAround->h / 2;
	}
	const int a = (int)( 255 * clamp( alpha, 0.0f, 1.0f ) );

	const int maxHealth = pGE->GetMaxHealth() > 0 ? pGE->GetMaxHealth() : 100;
	const float health = clamp( pGE->GetHealth(), 0, maxHealth );
	// Gameplays with no armour (max 0) show the "no armour" dial
	const int maxArmor = pGE->GetMaxArmor();
	const float armor = clamp( pGE->GetArmor(), 0, maxArmor > 0 ? maxArmor : MAX_ARMOR );

	const int armorBack = ( maxArmor == 0 && pArmorDeny ) ? pArmorDeny->textureId : pArmorEmpty->textureId;
	pArmorFull->DrawScew_Right( armorBack, w, h, GESS_CalcDialPoints( armor, maxArmor > 0 ? maxArmor : MAX_ARMOR ), offset, radius, cx, cy, a );
	pHealthFull->DrawScew_Left( pHealthEmpty->textureId, w, h, GESS_CalcDialPoints( health, maxHealth ), offset, radius, cx, cy, a );
}

void CGESplitScreenHud::Paint()
{
	UpdateFonts();
	CGESplitScreenClient *pSS = GESplitScreenClient();

	if ( !pSS->IsSplitActive() )
	{
		int w, h;
		surface()->GetScreenSize( w, h );
		GESSViewport_t full = { 0, 0, w, h };

		// One player per display: player 1 takes the whole screen before the match
		if ( pSS->IsPregame() )
		{
			PaintPregameSlot( 0, full );
			PaintPregameHelp( w, h );
			return;
		}

		if ( pSS->GetSlotMenu( 0 ) != GESS_MENU_NONE )
			PaintMenu( 0, full );

		PaintPadDisconnected( 0, full );

		// Split screen game with player 1 alone: their status instead of GE's spectator bars
		C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
		if ( pSS->ShouldSuppressGameMenus() && pLocal && ( pLocal->IsObserver() || pLocal->GetTeamNumber() == TEAM_SPECTATOR ) && pSS->GetSlotMenu( 0 ) == GESS_MENU_NONE )
			PaintObserverStatus( pLocal, full );

		// One small hint for everyone else
		if ( pSS->HasSparePad() )
		{
			DrawText( m_hFontSmall, w / 2, h * 3 / 4, Color( 255, 255, 255, 180 ),
				"Press START on another controller to join split screen", true );
		}
		return;
	}

	for ( int slot = 0; slot < GES_SS_MAX_PLAYERS; slot++ )
	{
		GESSViewport_t vp;
		if ( !pSS->GetViewport( slot, vp ) )
			continue;
		if ( pSS->IsPregame() )
			PaintPregameSlot( slot, vp );
		else
			PaintSlot( slot, vp );
	}

	// Cells nobody plays in: the scoreboard in the first, the logo in the rest
	for ( int i = 0; i < pSS->GetSpareCellCount(); i++ )
	{
		GESSViewport_t vp;
		if ( !pSS->GetSpareCell( i, vp ) )
			continue;
		if ( i == 0 )
		{
			// Nobody's view behind it: black, like the screen around the normal one
			surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
			surface()->DrawFilledRect( vp.x, vp.y, vp.x + vp.w, vp.y + vp.h );
			PaintScoreboard( vp );
		}
		else
			PaintLogo( vp );
	}

	// Dividers between the views
	surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
	for ( int slot = 0; slot < GES_SS_MAX_PLAYERS; slot++ )
	{
		GESSViewport_t vp;
		if ( pSS->GetViewport( slot, vp ) )
			surface()->DrawOutlinedRect( vp.x, vp.y, vp.x + vp.w, vp.y + vp.h );
	}
	for ( int i = 0; i < pSS->GetSpareCellCount(); i++ )
	{
		GESSViewport_t vp;
		if ( pSS->GetSpareCell( i, vp ) )
			surface()->DrawOutlinedRect( vp.x, vp.y, vp.x + vp.w, vp.y + vp.h );
	}

	if ( pSS->IsPregame() )
	{
		int w, h;
		surface()->GetScreenSize( w, h );
		PaintPregameHelp( w, h );
	}
}

//-----------------------------------------------------------------------------
// Before the match: each screen waits for its player
//-----------------------------------------------------------------------------
void CGESplitScreenHud::PaintPregameSlot( int slot, const GESSViewport_t &vp )
{
	CGESplitScreenClient *pSS = GESplitScreenClient();
	const int tallL = surface()->GetFontTall( m_hFontLarge );
	const int tallS = surface()->GetFontTall( m_hFontSmall );
	const int cx = vp.x + vp.w / 2;
	char title[64];
	Q_snprintf( title, sizeof( title ), "Player %d", slot + 1 );

	if ( pSS->IsPregameJoined( slot ) )
	{
		// In: their (frozen) view with a band saying so
		C_BasePlayer *pPlayer = pSS->GetSlotPlayer( slot );
		if ( !pPlayer || ( slot > 0 && pSS->GetSlotState( slot ) != GESS_SLOT_JOINED ) )
		{
			surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
			surface()->DrawFilledRect( vp.x, vp.y, vp.x + vp.w, vp.y + vp.h );
		}

		const int y = vp.y + vp.h / 2 - tallL;
		surface()->DrawSetColor( Color( 0, 0, 0, 170 ) );
		surface()->DrawFilledRect( vp.x, y - tallS, vp.x + vp.w, y + tallL + tallS * 3 );

		char line[128];
		const int team = pSS->GetSlotTeam( slot );
		const bool bTeams = GEMPRules() && GEMPRules()->IsTeamplay();
		Q_snprintf( line, sizeof( line ), "%s - ready%s", pSS->GetSlotName( slot ),
			bTeams ? ( team == TEAM_JANUS ? " (Janus)" : ( team == TEAM_MI6 ? " (MI6)" : "" ) ) : "" );
		DrawText( m_hFontLarge, cx, y, Color( 120, 220, 120, 255 ), line, true );
		DrawText( m_hFontSmall, cx, y + tallL + 4, Color( 255, 255, 255, 220 ), slot > 0 ? "LB / RB: change profile    B: leave" : "LB / RB: change profile", true );
		return;
	}

	surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
	surface()->DrawFilledRect( vp.x, vp.y, vp.x + vp.w, vp.y + vp.h );

	if ( slot != pSS->GetPregameSlot() )
	{
		// Waiting for the screens before it
		DrawText( m_hFontLarge, cx, vp.y + vp.h / 2 - tallL, Color( 130, 130, 130, 255 ), title, true );
		DrawText( m_hFontSmall, cx, vp.y + vp.h / 2 + 4, Color( 150, 150, 150, 255 ), "Next...", true );
		return;
	}

	// The screen being offered: pulse its outline
	const int a = 150 + (int)( 105.0f * sinf( (float) Plat_FloatTime() * 4.0f ) );
	surface()->DrawSetColor( Color( 230, 200, 80, a ) );
	for ( int i = 0; i < 3; i++ )
		surface()->DrawOutlinedRect( vp.x + i, vp.y + i, vp.x + vp.w - i, vp.y + vp.h - i );

	if ( pSS->GetPregameStage() == CGESplitScreenClient::GESS_PREGAME_CLAIM )
	{
		DrawText( m_hFontLarge, cx, vp.y + vp.h / 2 - tallL - tallS, Color( 230, 200, 80, 255 ), title, true );
		DrawText( m_hFontSmall, cx, vp.y + vp.h / 2 + 2, Color( 255, 255, 255, 230 ), "Press the RIGHT TRIGGER to take this screen", true );
		if ( slot == 0 )
			DrawText( m_hFontSmall, cx, vp.y + vp.h / 2 + tallS + 8, Color( 200, 200, 200, 200 ), "(player 1's controller)", true );
		return;
	}

	// Team: MI6 on the left, Janus on the right
	char line[128];
	Q_snprintf( line, sizeof( line ), "%s - %s", title, pSS->GetSlotName( slot ) );
	const int top = vp.y + vp.h / 2 - tallL * 2;
	DrawText( m_hFontLarge, cx, top, Color( 230, 200, 80, 255 ), line, true );

	const int boxW = Max( vp.w / 5, TextWide( m_hFontLarge, "Janus" ) + 24 ), boxH = tallL + 12;
	const int boxY = top + tallL + tallS;
	const int team = pSS->GetPregameTeam();
	for ( int i = 0; i < 2; i++ )
	{
		const bool bJanus = i == 1;
		const int bx = bJanus ? cx + 12 : cx - 12 - boxW;
		const bool bSel = ( team == TEAM_JANUS ) == bJanus;
		const Color teamCol = bJanus ? Color( 220, 70, 70, 255 ) : Color( 90, 140, 255, 255 );
		surface()->DrawSetColor( bSel ? Color( teamCol.r(), teamCol.g(), teamCol.b(), 200 ) : Color( 40, 40, 40, 255 ) );
		surface()->DrawFilledRect( bx, boxY, bx + boxW, boxY + boxH );
		surface()->DrawSetColor( bSel ? Color( 255, 230, 120, 255 ) : Color( 90, 90, 90, 255 ) );
		surface()->DrawOutlinedRect( bx, boxY, bx + boxW, boxY + boxH );
		if ( bSel )
			surface()->DrawOutlinedRect( bx - 1, boxY - 1, bx + boxW + 1, boxY + boxH + 1 );
		DrawText( m_hFontLarge, bx + boxW / 2, boxY + 6, Color( 255, 255, 255, 255 ), bJanus ? "Janus" : "MI6", true );
	}
	DrawText( m_hFontSmall, cx, boxY + boxH + tallS / 2, Color( 255, 255, 255, 220 ), "Left stick: choose team    RIGHT TRIGGER: confirm", true );
}

void CGESplitScreenHud::PaintPregameHelp( int w, int h )
{
	const char *text = GESplitScreenClient()->IsPregameJoined( 0 ) ?
		"Waiting for everyone to join - player 1: START begins with the players who are in" :
		"Waiting for the players to take their screens";
	const int tall = surface()->GetFontTall( m_hFontSmall );
	const int tw = TextWide( m_hFontSmall, text );
	surface()->DrawSetColor( Color( 0, 0, 0, 200 ) );
	surface()->DrawFilledRect( w / 2 - tw / 2 - 12, h - tall * 2 - 8, w / 2 + tw / 2 + 12, h - 4 );
	DrawText( m_hFontSmall, w / 2, h - tall * 2 - 2, Color( 230, 200, 80, 255 ), text, true );
}

//-----------------------------------------------------------------------------
// Spare cells
//-----------------------------------------------------------------------------
void CGESplitScreenHud::PaintScoreboard( const GESSViewport_t &vp, int highlightPlayer )
{
	CGESplitScreenClient *pSS = GESplitScreenClient();

	// Before the match: who is in so far
	if ( pSS->IsPregame() )
	{
		// GoldenEye's paper look
		surface()->DrawSetColor( Color( 214, 202, 170, 255 ) );
		surface()->DrawFilledRect( vp.x, vp.y, vp.x + vp.w, vp.y + vp.h );
		surface()->DrawSetColor( Color( 150, 135, 100, 255 ) );
		surface()->DrawOutlinedRect( vp.x + 6, vp.y + 6, vp.x + vp.w - 6, vp.y + vp.h - 6 );

		const int pad = Max( 8, vp.w / 40 );
		const int tallL = surface()->GetFontTall( m_hFontLarge );
		const int tallS = surface()->GetFontTall( m_hFontSmall );
		const Color ink( 40, 32, 20, 255 );
		int y = vp.y + pad;
		DrawText( m_hFontLarge, vp.x + pad, y, ink, "Players in" );
		y += tallL + 6;
		bool bAny = false;
		for ( int slot = 0; slot < GES_SS_MAX_PLAYERS; slot++ )
		{
			if ( !pSS->IsPregameJoined( slot ) )
				continue;
			char line[96];
			Q_snprintf( line, sizeof( line ), "Player %d: %s", slot + 1, pSS->GetSlotName( slot ) );
			DrawText( m_hFontSmall, vp.x + pad, y, ink, line );
			y += tallS + 4;
			bAny = true;
		}
		if ( !bAny )
			DrawText( m_hFontSmall, vp.x + pad, y, Color( 90, 80, 60, 255 ), "Nobody yet" );
		return;
	}

	PaintGEScoreboard( vp, highlightPlayer );
}

//-----------------------------------------------------------------------------
// GoldenEye's own scoreboard (CGEScoreBoard, resource/ui/scoreboard.res), drawn
// the same way inside a part of the screen: its background, fonts, colours,
// sections, columns and footer, laid out from the .res file and scaled to fit.
// The vgui panel itself can't be drawn into a split view: it's one full-screen
// panel and its fonts don't scale.
//-----------------------------------------------------------------------------
HFont CGESplitScreenHud::GetScaledFont( const char *pszFace, int tall, int weight, int flags )
{
	tall = clamp( tall, 6, 160 );
	for ( int i = 0; i < m_ScaledFonts.Count(); i++ )
	{
		const ScaledFont_t &f = m_ScaledFonts[i];
		if ( f.tall == tall && f.weight == weight && f.flags == flags && !Q_strcmp( f.face, pszFace ) )
			return f.font;
	}

	ScaledFont_t f;
	Q_strncpy( f.face, pszFace, sizeof( f.face ) );
	f.tall = tall;
	f.weight = weight;
	f.flags = flags;
	f.font = surface()->CreateFont();
	if ( !surface()->SetFontGlyphSet( f.font, pszFace, tall, weight, 0, 0, flags ) )
		f.font = m_hFontSmall;
	m_ScaledFonts.AddToTail( f );
	return f.font;
}

static void GESS_DrawTextW( HFont font, int x, int y, Color col, const wchar_t *text )
{
	surface()->DrawSetTextFont( font );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawPrintText( text, wcslen( text ) );
}

static int GESS_TextWideW( HFont font, const wchar_t *text )
{
	int tw, th;
	surface()->GetTextSize( font, text, tw, th );
	return tw;
}

// Text in a label box of the .res: centred vertically, aligned left / centre / right
static void GESS_DrawLabelW( HFont font, int x, int y, int w, int h, Color col, const wchar_t *text, int align )
{
	const int tw = GESS_TextWideW( font, text );
	const int tx = align < 0 ? x : ( align > 0 ? x + w - tw : x + ( w - tw ) / 2 );
	GESS_DrawTextW( font, tx, y + ( h - surface()->GetFontTall( font ) ) / 2, col, text );
}

static void GESS_DrawLabel( HFont font, int x, int y, int w, int h, Color col, const char *text, int align )
{
	wchar_t wsz[256];
	g_pVGuiLocalize->ConvertANSIToUnicode( text, wsz, sizeof( wsz ) );
	GESS_DrawLabelW( font, x, y, w, h, col, wsz, align );
}

static const wchar_t *GESS_Localize( const char *token, const wchar_t *fallback )
{
	const wchar_t *w = g_pVGuiLocalize->Find( token );
	return w ? w : fallback;
}

struct GESSBoardRow_t
{
	int index, score, deaths, team;
	char scoreText[16];
};

static int GESS_BoardSort( const GESSBoardRow_t *a, const GESSBoardRow_t *b )
{
	// CGEScoreBoard::StaticPlayerSortFunc: score (or time) down, deaths up
	if ( a->score != b->score )
		return b->score - a->score;
	if ( a->deaths != b->deaths )
		return a->deaths - b->deaths;
	return a->index - b->index;
}

void CGESplitScreenHud::PaintGEScoreboard( const GESSViewport_t &area, int highlightPlayer )
{
	C_GEPlayerResource *pRes = GEPlayerRes();
	if ( !pRes || !GEMPRules() )
		return;

	// The panel is 420 x 340 (in 640 x 480 units) and its background texture
	// reaches 44 above it (422 x 425 in all), but the green screen you see only
	// covers units 40 to 302 down: fit that visible part into the area, centred.
	const float bw = 422.0f, visTop = 40.0f, visBottom = 302.0f, bh = visBottom - visTop;
	const float s = Min( area.w / bw, area.h / bh );
	if ( s <= 0.05f )
		return;
	const float ox = area.x + ( area.w - bw * s ) * 0.5f;
	const float oy = area.y + ( area.h - bh * s ) * 0.5f - visTop * s;
#define SBX( u ) ( (int)( ox + (u) * s + 0.5f ) )
#define SBY( u ) ( (int)( oy + (u) * s + 0.5f ) )
#define SBS( u ) ( (int)( (u) * s + 0.5f ) )

	const bool bTeams = GEMPRules()->IsTeamplay();

	// Background (the green radar screen)
	static int s_iBgFFA = -1, s_iBgTeam = -1;
	if ( s_iBgFFA < 0 )
	{
		s_iBgFFA = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( s_iBgFFA, "vgui/scoreboard/background_ffa", true, false );
		s_iBgTeam = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( s_iBgTeam, "vgui/scoreboard/background_team", true, false );
	}
	surface()->DrawSetTexture( bTeams ? s_iBgTeam : s_iBgFFA );
	surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
	surface()->DrawTexturedRect( SBX( 0 ), SBY( -44 ), SBX( 422 ), SBY( 381 ) );

	// Fonts: "Scoreboard" and "Scoreboard_Spec" (Adventure Subtitles GES, 8 and 6
	// units), the rows' "DefaultVerySmall" (Tahoma, 14 pixels on a 1080 screen)
	const int aa = ISurface::FONTFLAG_ANTIALIAS | ISurface::FONTFLAG_CUSTOM;
	// Split screen views are small and the game is played from the sofa: the rows are at least
	// a 21st of the board's height (22 pixels in a quarter of a 1080p screen), the headers
	// nearly as big, and the rows spaced to fit them
	const int rowTall = (int)( Max( Max( 14.0f * s / 2.25f, 9.0f * s ), area.h / 21.0f ) + 0.5f );
	HFont fBoard = GetScaledFont( "Adventure Subtitles GES", Max( SBS( 8 ), rowTall * 9 / 10 ), 300, aa );
	HFont fSpec = GetScaledFont( "Adventure Subtitles GES", SBS( 6 ), 100, aa );
	// (in a small view the rows get as tall a font as their 10-unit spacing allows, to stay readable)
	HFont fRow = GetScaledFont( "Tahoma", rowTall, 0, ISurface::FONTFLAG_ANTIALIAS );

	// Colours from the scheme (ClientScheme.res)
	const Color colText( 153, 255, 140, 255 );		// SB_Text / SB_Labels
	const Color colSelText( 206, 224, 204, 255 );	// SB_SelText
	const Color colSelBg( 255, 255, 255, 80 );		// the selected row's highlight
	const Color colSpec( 220, 220, 220, 200 );		// Scoreboard_Spectators

	// Server name across the top
	{
		static ConVarRef hostname( "hostname" );
		GESS_DrawLabel( fBoard, SBX( 76 ), SBY( 43 ), SBS( 270 ), SBS( 12 ), colText, hostname.IsValid() ? hostname.GetString() : "", 0 );
	}

	// Team scores (team games)
	if ( bTeams )
	{
		const wchar_t *fmt = GESS_Localize( "#ScoreBoard_TeamScore", L"Score: %s1" );
		const int teams[2] = { TEAM_MI6, TEAM_JANUS };
		const int ys[2] = { 53, 170 };
		for ( int t = 0; t < 2; t++ )
		{
			C_Team *pTeam = GetGlobalTeam( teams[t] );
			if ( !pTeam )
				continue;
			wchar_t wszRounds[8], wszScore[64];
			V_snwprintf( wszRounds, ARRAYSIZE( wszRounds ), L"%i", pTeam->GetRoundScore() );
			g_pVGuiLocalize->ConstructString( wszScore, sizeof( wszScore ), fmt, 1, wszRounds );
			GESS_DrawLabelW( fBoard, SBX( 105 ), SBY( ys[t] ), SBS( 80 ), SBS( 10 ), pRes->GetTeamColor( teams[t] ), wszScore, 0 );
		}
	}

	// The player list: sections (one, or MI6 and Janus), a header row each, then
	// the players. Columns as in CGEScoreBoard::AddSection.
	const float listX = 48 + 4.7f, listY = 46 + 4.1f, listW = 364 - listX, listBottom = 46 + 240;
	// No Ping column (everyone is on this PC): its room goes to the others
	const float colW[6] = { 16, 136, 62, 46, 46, 0 };	// dead icon, name, identity, score, deaths, (ping)
	float colX[6];
	colX[0] = listX;
	for ( int c = 1; c < 6; c++ )
		colX[c] = colX[c - 1] + colW[c - 1];
	const float lineSpacing = Max( 10.0f, surface()->GetFontTall( fRow ) * 1.2f / s ), sectionGap = 8.0f;
	const float headerTall = surface()->GetFontTall( fBoard ) / s + 7.0f;

	CUtlVector<GESSBoardRow_t> rows;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		if ( !pRes->IsConnected( i ) )
			continue;
		GESSBoardRow_t row;
		row.index = i;
		row.team = pRes->GetTeam( i );
		row.deaths = pRes->GetDeaths( i );
		row.score = pRes->GetPlayerScore( i );
		if ( GEMPRules()->GetScoreboardMode() )
			Q_snprintf( row.scoreText, sizeof( row.scoreText ), "%d:%02d", row.score / 60, row.score % 60 );
		else
			Q_snprintf( row.scoreText, sizeof( row.scoreText ), "%d", row.score );
		rows.AddToTail( row );
	}
	rows.Sort( GESS_BoardSort );

	CHudTexture *pDeadIcons[8] = {};
	static const char *s_pszIcons[8] = { NULL, "scoreboard/player_dead", "scoreboard/ges_dev", "scoreboard/ges_bt", "scoreboard/ges_dev_dead", "scoreboard/ges_silverach", "scoreboard/ges_goldach", "scoreboard/ges_cc" };
	static int s_iIcons[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
	for ( int i = 1; i < 8; i++ )
	{
		if ( s_iIcons[i] < 0 )
		{
			s_iIcons[i] = surface()->CreateNewTextureID();
			surface()->DrawSetTextureFile( s_iIcons[i], VarArgs( "vgui/%s", s_pszIcons[i] ), true, false );
		}
	}
	(void) pDeadIcons;

	const int numSections = bTeams ? 2 : 1;
	const int sectionTeams[2] = { bTeams ? TEAM_MI6 : TEAM_UNASSIGNED, TEAM_JANUS };
	float y = listY;
	for ( int sec = 0; sec < numSections; sec++ )
	{
		const int team = sectionTeams[sec];
		if ( sec > 0 )
			y += sectionGap;
		const float sectionTop = y;

		// Header: "Players (n)" or "MI6 (n)", and the column titles
		int numPlayers = 0;
		for ( int r = 0; r < rows.Count(); r++ )
			numPlayers += ( !bTeams || rows[r].team == team ) && rows[r].team != TEAM_SPECTATOR ? 1 : 0;
		C_Team *pTeam = GetGlobalTeam( team );
		if ( pTeam )
			numPlayers = pTeam->Get_Number_Players();
		wchar_t wszNum[8], wszHead[128];
		V_snwprintf( wszNum, ARRAYSIZE( wszNum ), L"%i", numPlayers );
		if ( !bTeams )
		{
			g_pVGuiLocalize->ConstructString( wszHead, sizeof( wszHead ), GESS_Localize( "#ScoreBoard_FFA", L"Players (%s1)" ), 1, wszNum );
		}
		else
		{
			wchar_t wszName[64];
			g_pVGuiLocalize->ConvertANSIToUnicode( pTeam ? pTeam->Get_Name() : "", wszName, sizeof( wszName ) );
			g_pVGuiLocalize->ConstructString( wszHead, sizeof( wszHead ), GESS_Localize( "#ScoreBoard_TeamPlay", L"%s1 (%s2)" ), 2, wszName, wszNum );
		}
		const Color headCol = bTeams ? pRes->GetTeamColor( team ) : colText;
		const int hy = SBY( y ), hh = SBS( headerTall );
		GESS_DrawLabelW( fBoard, SBX( colX[1] ), hy, SBS( colW[1] ), hh, headCol, wszHead, -1 );
		GESS_DrawLabelW( fBoard, SBX( colX[2] ), hy, SBS( colW[2] ), hh, headCol, GESS_Localize( "#GE_Charname", L"Identity" ), 0 );
		GESS_DrawLabelW( fBoard, SBX( colX[3] ), hy, SBS( colW[3] ), hh, headCol, GESS_Localize( "#GE_Score", L"Score" ), 0 );
		GESS_DrawLabelW( fBoard, SBX( colX[4] ), hy, SBS( colW[4] ), hh, headCol, GESS_Localize( "#GE_Deaths", L"Deaths" ), 0 );
		y += headerTall;

		for ( int r = 0; r < rows.Count(); r++ )
		{
			const GESSBoardRow_t &row = rows[r];
			if ( row.team == TEAM_SPECTATOR || ( bTeams && row.team != team ) )
				continue;
			if ( y + lineSpacing > listBottom )
				break;

			const int ry = SBY( y ), rh = SBY( y + lineSpacing ) - ry;
			const int i = row.index;

			// Text colour: the gameplay's choice for this player, else their team's
			Color col = pRes->GetTeamColor( row.team );
			switch ( pRes->GetScoreBoardColor( i ) )
			{
			case SB_COLOR_GOLD:			col = Color( 232, 180, 2, 255 ); break;
			case SB_COLOR_WHITE:		col = Color( 220, 220, 220, 255 ); break;
			case SB_COLOR_ELIMINATED:	col = bTeams ? Color( 0, 0, 0, 255 ) : Color( 255, 0, 0, 255 ); break;
			}

			// The viewer's own row is highlighted
			if ( i == highlightPlayer )
			{
				surface()->DrawSetColor( colSelBg );
				surface()->DrawFilledRect( SBX( listX ), ry, SBX( 364 ), ry + rh );
				col = colSelText;
			}

			// Dead / developer icon
			int icon = 0;
			const bool bAlive = pRes->IsAlive( i );
			switch ( pRes->GetDevStatus( i ) )
			{
			case GE_DEVELOPER:		icon = bAlive ? 2 : 4; break;
			case GE_BETATESTER:		icon = bAlive ? 3 : 4; break;
			case GE_CONTRIBUTOR:	icon = bAlive ? 7 : 1; break;
			case GE_SILVERACH:		icon = bAlive ? 5 : 1; break;
			case GE_GOLDACH:		icon = bAlive ? 6 : 1; break;
			default:				icon = bAlive ? 0 : 1; break;
			}
			if ( icon > 0 && s_iIcons[icon] >= 0 )
			{
				const int isz = Min( SBS( colW[0] ), rh );
				const int ix = SBX( colX[0] + colW[0] * 0.5f ) - isz / 2;
				surface()->DrawSetTexture( s_iIcons[icon] );
				surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
				surface()->DrawTexturedRect( ix, ry + ( rh - isz ) / 2, ix + isz, ry + ( rh - isz ) / 2 + isz );
			}

			char name[64];
			Q_strncpy( name, pRes->GetPlayerName( i ), sizeof( name ) );
			GESS_DrawLabel( fRow, SBX( colX[1] ), ry, SBS( colW[1] - 2 ), rh, col, GEUTIL_RemoveColorHints( name ), -1 );
			{
				// The character's short name (a localisation token, as the list shows it)
				const char *pszChar = pRes->GetCharName( i );
				const wchar_t *pwszChar = ( pszChar && pszChar[0] == '#' ) ? g_pVGuiLocalize->Find( pszChar ) : NULL;
				if ( pwszChar )
					GESS_DrawLabelW( fRow, SBX( colX[2] ), ry, SBS( colW[2] ), rh, col, pwszChar, 0 );
				else
					GESS_DrawLabel( fRow, SBX( colX[2] ), ry, SBS( colW[2] ), rh, col, pszChar ? pszChar : "", 0 );
			}
			GESS_DrawLabel( fRow, SBX( colX[3] ), ry, SBS( colW[3] ), rh, col, row.scoreText, 0 );
			GESS_DrawLabel( fRow, SBX( colX[4] ), ry, SBS( colW[4] ), rh, col, VarArgs( "%d", row.deaths ), 0 );

			y += lineSpacing;
		}

		// MI6 always takes at least this much room, so Janus lines up with the background
		if ( bTeams && sec == 0 && y < sectionTop + 113 )
			y = sectionTop + 113;
	}

	// Spectators
	C_Team *pSpec = GetGlobalTeam( TEAM_SPECTATOR );
	if ( pSpec && pSpec->Get_Number_Players() > 0 )
	{
		wchar_t wszNames[1024] = L"", wszPlayer[MAX_PLAYER_NAME_LENGTH], wszSpec[1100];
		for ( int i = 0; i < pSpec->Get_Number_Players(); i++ )
		{
			if ( i > 0 )
				V_wcsncat( wszNames, L", ", ARRAYSIZE( wszNames ) );
			char name[64];
			Q_strncpy( name, UTIL_SafeName( pRes->GetPlayerName( pSpec->m_aPlayers[i] ) ), sizeof( name ) );
			g_pVGuiLocalize->ConvertANSIToUnicode( GEUTIL_RemoveColorHints( name ), wszPlayer, sizeof( wszPlayer ) );
			V_wcsncat( wszNames, wszPlayer, ARRAYSIZE( wszNames ) );
		}
		g_pVGuiLocalize->ConstructString( wszSpec, sizeof( wszSpec ), GESS_Localize( "#ScoreBoard_Spectators", L"Spectators: %s1" ), 1, wszNames );
		GESS_DrawLabelW( fSpec, SBX( 68 ), SBY( 280 ), SBS( 270 ), SBS( 11 ), colSpec, wszSpec, -1 );
	}

	// Footer: map, time left, game mode
	{
		char szMap[64];
		Q_FileBase( engine->GetLevelName(), szMap, sizeof( szMap ) );
		GESS_DrawLabel( fBoard, SBX( 75 ), SBY( 290 ), SBS( 117 ), SBS( 11 ), colText, szMap, 0 );

		char szTime[16] = "--:--";
		const float timeleft = GEMPRules()->GetMatchTimeRemaining();
		if ( timeleft > 0 )
		{
			const int mins = (int) timeleft / 60;
			const int secs = (int) timeleft - mins * 60;
			Q_snprintf( szTime, sizeof( szTime ), "%d:%.2d", mins, secs );
		}
		GESS_DrawLabel( fBoard, SBX( 193 ), SBY( 291 ), SBS( 36 ), SBS( 8 ), colText, szTime, 0 );

		wchar_t wszGameplay[64];
		GEUTIL_GetGameplayName( wszGameplay, sizeof( wszGameplay ) );
		GESS_DrawLabelW( fBoard, SBX( 230 ), SBY( 290 ), SBS( 117 ), SBS( 11 ), colText, wszGameplay, 0 );
	}

#undef SBX
#undef SBY
#undef SBS
}

//-----------------------------------------------------------------------------
// GoldenEye's own ammo counter (CGEHudAmmo, "GEAmmo" in HudLayout.res): the
// clip in AmmoNumbers (Adventure Subtitles GES 17, drop shadow), the ammo
// type's icon, then the ammo left, in the bottom right of the view. Hidden for
// weapons without ammo; the Moonraker only shows its icon. Laid out in 640 x
// 480 units like the normal HUD, for this player's view.
//-----------------------------------------------------------------------------
void CGESplitScreenHud::PaintGEAmmo( int slot, C_BasePlayer *pPlayer, const GESSViewport_t &vp )
{
	static ConVarRef cl_ge_show_ammocount( "cl_ge_show_ammocount" );
	if ( cl_ge_show_ammocount.IsValid() && !cl_ge_show_ammocount.GetBool() )
		return;

	C_GEWeapon *pWeapon = ToGEWeapon( pPlayer->GetActiveWeapon() );
	if ( !pWeapon || !pWeapon->UsesPrimaryAmmo() )
		return;

	// This player's clip and ammo (a guest's come with their player: ammo counts
	// are only sent to the player they belong to)
	int clip, total;
	C_GEMPPlayer *pMP = ToGEMPPlayer( pPlayer );
	if ( slot == 0 || !pMP )
	{
		clip = pWeapon->Clip1();
		total = pPlayer->GetAmmoCount( pWeapon->GetPrimaryAmmoType() );
	}
	else
	{
		clip = pMP->GetSplitClip();
		total = pMP->GetSplitAmmo();
	}
	if ( clip < 0 )
	{
		// No clip: the ammo left is the main number
		clip = total;
		total = 0;
	}
	const bool bShowValue = pWeapon->GetWeaponID() != WEAPON_MOONRAKER;
	const bool bShowTotal = bShowValue && pWeapon->UsesClipsForAmmo1();

	// Proportional units for this view (the normal HUD's are for the screen height)
	const float s = Min( vp.h / 480.0f, vp.w / 640.0f );
	const int x0 = vp.x + vp.w - (int)( 130 * s );
	const int y0 = vp.y + (int)( 416 * s );
	const int digitY = y0 + (int)( 34 * s );

	HFont font = GetScaledFont( "Adventure Subtitles GES", (int)( 17 * s + 0.5f ), 30,
		ISurface::FONTFLAG_ANTIALIAS | ISurface::FONTFLAG_DROPSHADOW | ISurface::FONTFLAG_CUSTOM );
	const Color col( 255, 255, 255, 255 );	// Panel.FgColor

	const int charWidth = surface()->GetCharacterWidth( font, '0' );
	const int charHeight = surface()->GetFontTall( font );
	const int clipX = clip < 10 ? x0 + charWidth : x0;

	// The ammo type's icon, sized for this view like the normal HUD on this screen
	int iconX = 0, iconW = 0;
	CHudTexture *pIcon = gWR.GetAmmoIconFromWeapon( pWeapon->GetPrimaryAmmoType() );
	if ( pIcon )
	{
		int sw, sh;
		surface()->GetScreenSize( sw, sh );
		const float k = s / ( sh / 480.0f );
		iconW = (int)( pIcon->Width() * k + 0.5f );
		const int iconH = (int)( pIcon->Height() * k + 0.5f );
		iconX = clip < 10 ? clipX + charWidth + (int)( 3 * k ) : clipX + charWidth * 2 + (int)( 3 * k );
		const int iconY = ( digitY + charHeight ) - iconH - (int)( 2 * s );
		pIcon->DrawSelf( iconX, iconY, iconW, iconH, col );
	}
	const int totalX = iconX + iconW + 3;

	if ( bShowValue )
		DrawText( font, clipX, digitY, col, VarArgs( "%d", clip ) );
	if ( bShowTotal )
		DrawText( font, totalX, digitY, col, VarArgs( "%d", total ) );
}

void CGESplitScreenHud::PaintLogo( const GESSViewport_t &vp )
{
	surface()->DrawSetColor( Color( 12, 12, 12, 255 ) );
	surface()->DrawFilledRect( vp.x, vp.y, vp.x + vp.w, vp.y + vp.h );
	const int tallL = surface()->GetFontTall( m_hFontLarge );
	DrawText( m_hFontLarge, vp.x + vp.w / 2, vp.y + vp.h / 2 - tallL, Color( 210, 170, 60, 255 ), "GOLDENEYE", true );
	DrawText( m_hFontSmall, vp.x + vp.w / 2, vp.y + vp.h / 2 + 2, Color( 200, 200, 200, 200 ), "SOURCE", true );
}

// Split screen players are never spectators: an observer is either still joining, or
// out until the next round in a mode that eliminates players (You Only Live Twice)
void CGESplitScreenHud::PaintObserverStatus( C_BasePlayer *pPlayer, const GESSViewport_t &vp )
{
	const bool bJoining = pPlayer->GetTeamNumber() == TEAM_SPECTATOR;
	const int tall = surface()->GetFontTall( m_hFontLarge );
	const int y = vp.y + vp.h / 2 - tall;
	surface()->DrawSetColor( Color( 0, 0, 0, 150 ) );
	surface()->DrawFilledRect( vp.x, y - tall / 2, vp.x + vp.w, y + tall * 2 );
	DrawText( m_hFontLarge, vp.x + vp.w / 2, y, Color( 230, 200, 80, 255 ), bJoining ? "Joining..." : "Out - waiting for the next round", true );
}

// A player whose controller was unplugged keeps their place until one presses a button
void CGESplitScreenHud::PaintPadDisconnected( int slot, const GESSViewport_t &vp )
{
	if ( !GESplitScreenClient()->IsSlotPadDisconnected( slot ) )
		return;

	const int tall = surface()->GetFontTall( m_hFontLarge );
	const int y = vp.y + vp.h / 3;
	surface()->DrawSetColor( Color( 0, 0, 0, 170 ) );
	surface()->DrawFilledRect( vp.x, y - tall, vp.x + vp.w, y + tall * 2 );
	DrawText( m_hFontLarge, vp.x + vp.w / 2, y - tall / 2, Color( 230, 200, 80, 255 ), "Controller disconnected", true );
	DrawText( m_hFontSmall, vp.x + vp.w / 2, y + tall / 2 + 4, Color( 255, 255, 255, 220 ), "Press a button on a controller to carry on", true );
}

void CGESplitScreenHud::PaintJoinHint( const GESSViewport_t &vp, const char *line1, const char *line2 )
{
	surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
	surface()->DrawFilledRect( vp.x, vp.y, vp.x + vp.w, vp.y + vp.h );

	const int tall = surface()->GetFontTall( m_hFontLarge );
	DrawText( m_hFontLarge, vp.x + vp.w / 2, vp.y + vp.h / 2 - tall, Color( 230, 200, 80, 255 ), line1, true );
	if ( line2 )
		DrawText( m_hFontSmall, vp.x + vp.w / 2, vp.y + vp.h / 2 + 4, Color( 255, 255, 255, 200 ), line2, true );
}

void CGESplitScreenHud::PaintSlot( int slot, const GESSViewport_t &vp )
{
	CGESplitScreenClient *pSS = GESplitScreenClient();
	const GESSSlotState_t state = slot == 0 ? GESS_SLOT_JOINED : pSS->GetSlotState( slot );
	C_BasePlayer *pPlayer = pSS->GetSlotPlayer( slot );

	char title[64];
	Q_snprintf( title, sizeof( title ), "Player %d", slot + 1 );

	if ( state == GESS_SLOT_EMPTY )
	{
		const int pad = pSS->GetSlotPad( slot );
		const bool bPad = ( pad >= 0 && GESSGamepads()->GetPad( pad ).connected ) || pSS->HasSparePad();
		PaintJoinHint( vp, title, bPad ? "Press START to join" : "Connect a controller to join" );
		return;
	}

	if ( state == GESS_SLOT_CHOOSING || ( state == GESS_SLOT_JOINING && !pPlayer ) )
	{
		PaintJoinHint( vp, title, state == GESS_SLOT_JOINING ? "Joining..." : NULL );
	}
	else if ( pPlayer )
	{
		PaintPlayerHud( slot, pPlayer, vp );
	}

	PaintPadDisconnected( slot, vp );

	if ( pSS->GetSlotMenu( slot ) != GESS_MENU_NONE )
		PaintMenu( slot, vp );
}

void CGESplitScreenHud::PaintPlayerHud( int slot, C_BasePlayer *pPlayer, const GESSViewport_t &vp )
{
	CGESplitScreenClient *pSS = GESplitScreenClient();
	C_GEPlayer *pGE = ToGEPlayer( pPlayer );
	C_GEMPPlayer *pMP = ToGEMPPlayer( pPlayer );

	const int pad = Max( 4, vp.h / 40 );

	// Name in the top left
	DrawText( m_hFontSmall, vp.x + pad, vp.y + pad, Color( 255, 255, 255, 220 ), pPlayer->GetPlayerName() );

	// Time left, top centre (the full-screen round timer would sit across other views)
	if ( GEMPRules() && !GEMPRules()->IsIntermission() )
	{
		float timeleft = 0;
		if ( GEMPRules()->IsRoundTimeRunning() )
			timeleft = GEMPRules()->GetRoundTimeRemaining();
		else if ( GEMPRules()->IsMatchTimeRunning() )
			timeleft = GEMPRules()->GetMatchTimeRemaining();
		if ( timeleft > 0 )
		{
			const int secs = (int) ceilf( timeleft );
			char szTime[16];
			Q_snprintf( szTime, sizeof( szTime ), "%d:%02d", secs / 60, secs % 60 );
			DrawText( m_hFontLarge, vp.x + vp.w / 2, vp.y + pad, secs <= 30 ? Color( 255, 90, 90, 240 ) : Color( 255, 255, 255, 230 ), szTime, true );
		}
	}

	if ( pPlayer->IsObserver() || pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
	{
		PaintObserverStatus( pPlayer, vp );
		return;
	}

	if ( !pPlayer->IsAlive() )
	{
		DrawText( m_hFontLarge, vp.x + vp.w / 2, vp.y + vp.h / 2, Color( 255, 80, 80, 230 ), "Press FIRE to respawn", true );
		return;
	}

	// Crosshair
	if ( pSS->GetSlotMenu( slot ) == GESS_MENU_NONE )
	{
		const int cx = vp.x + vp.w / 2, cy = vp.y + vp.h / 2;
		const int len = Max( 4, vp.h / 60 ), gap = Max( 2, vp.h / 160 );
		surface()->DrawSetColor( Color( 255, 255, 255, 200 ) );
		surface()->DrawFilledRect( cx - gap - len, cy, cx - gap, cy + 1 );
		surface()->DrawFilledRect( cx + gap + 1, cy, cx + gap + 1 + len, cy + 1 );
		surface()->DrawFilledRect( cx, cy - gap - len, cx + 1, cy - gap );
		surface()->DrawFilledRect( cx, cy + gap + 1, cx + 1, cy + gap + 1 + len );
	}

	// Health and armour: GoldenEye's own dials either side of the crosshair, shown
	// when they change (hit, healed, armour picked up, spawned) and fading out
	// like the normal game's, in this player's view with their values
	float barAlpha = UpdateBarVisibility( slot, pPlayer, pPlayer->GetHealth(), pGE ? pGE->GetArmor() : 0 );
	// With the scoreboard up (their START menu's leaderboard) they stay on screen,
	// as holding the scoreboard does in the normal game, and fade out after it
	if ( pSS->GetSlotMenu( slot ) == GESS_MENU_PAUSE && pSS->GetSlotMenuSelection( slot ) == GESS_GAMEPAGE_LEADERBOARD )
	{
		m_Bars[slot].showUntil = Max( m_Bars[slot].showUntil, gpGlobals->realtime + 1.0f );
		barAlpha = 1.0f;
	}
	// (the leaderboard draws them itself, larger and wrapped around it)
	if ( barAlpha > 0.0f && pGE && !( pSS->GetSlotMenu( slot ) == GESS_MENU_PAUSE && pSS->GetSlotMenuSelection( slot ) == GESS_GAMEPAGE_LEADERBOARD ) )
		PaintLifeDials( pGE, vp, barAlpha );

	// GoldenEye's radar, centred on this player (ge_hudradar.cpp), when the match allows it
	// (not under the leaderboard)
	if ( !( pSS->GetSlotMenu( slot ) == GESS_MENU_PAUSE && pSS->GetSlotMenuSelection( slot ) == GESS_GAMEPAGE_LEADERBOARD ) )
		GESS_PaintRadar( slot, pPlayer, vp.x, vp.y, vp.w, vp.h );

	// Ammo: GoldenEye's own counter, bottom right
	(void) pMP;
	PaintGEAmmo( slot, pPlayer, vp );

	// Score in the top right, or the match goal when kills / K:D decide the winner
	static ConVarRef ge_wincondition( "ge_wincondition" );
	static ConVarRef ge_killlimit( "ge_killlimit" );
	C_GEMPPlayer *pMPPlayer = ToGEMPPlayer( pPlayer );
	const int wincond = ge_wincondition.IsValid() ? ge_wincondition.GetInt() : 0;

	char szScore[48] = "";
	if ( wincond == 1 && pMPPlayer )
	{
		const int limit = ge_killlimit.IsValid() ? ge_killlimit.GetInt() : 0;
		if ( limit > 0 )
			Q_snprintf( szScore, sizeof( szScore ), "Kills %d / %d", pMPPlayer->GetMatchKills(), limit );
		else
			Q_snprintf( szScore, sizeof( szScore ), "Kills %d", pMPPlayer->GetMatchKills() );
	}
	else if ( wincond == 2 && pMPPlayer )
	{
		const int deaths = pMPPlayer->GetMatchKDDeaths();
		Q_snprintf( szScore, sizeof( szScore ), "K/D %.2f (%d/%d)", (float) pMPPlayer->GetMatchKills() / (float) MAX( deaths, 1 ),
			pMPPlayer->GetMatchKills(), deaths );
	}
	else if ( g_PR )
	{
		Q_snprintf( szScore, sizeof( szScore ), "Score %d", g_PR->GetPlayerScore( pPlayer->entindex() ) );
	}

	if ( szScore[0] )
	{
		const int tw = TextWide( m_hFontSmall, szScore );
		DrawText( m_hFontSmall, vp.x + vp.w - pad - tw, vp.y + pad, Color( 255, 255, 255, 180 ), szScore );
	}

	// Team scores under it in team games (the full-screen team score box would sit in
	// someone else's view)
	if ( GEMPRules() && GEMPRules()->IsTeamplay() )
	{
		C_Team *pMI6 = GetGlobalTeam( TEAM_MI6 ), *pJanus = GetGlobalTeam( TEAM_JANUS );
		if ( pMI6 && pJanus )
		{
			char szTeams[64];
			Q_snprintf( szTeams, sizeof( szTeams ), "MI6 %d  Janus %d", pMI6->GetRoundScore(), pJanus->GetRoundScore() );
			const int tw = TextWide( m_hFontSmall, szTeams );
			const Color col = pPlayer->GetTeamNumber() == TEAM_MI6 ? Color( 120, 170, 255, 220 ) : Color( 255, 110, 110, 220 );
			DrawText( m_hFontSmall, vp.x + vp.w - pad - tw, vp.y + pad + surface()->GetFontTall( m_hFontSmall ) + 2, col, szTeams );
		}
	}
}

//-----------------------------------------------------------------------------
// The mouse in player 1's START menu
//
// While it's open (CGESplitScreenClient::WantsMenuMouse) an invisible popup over the
// screen takes the mouse: vgui shows the cursor for it, and clicks go to it instead
// of firing. Player 1's menu records where it drew its buttons each frame; the
// popup looks up what's under the cursor and tells the split screen client.
//-----------------------------------------------------------------------------
struct GESSMenuHit_t
{
	int x0, y0, x1, y1;
	int hit, arg;
};
static CUtlVector<GESSMenuHit_t> s_MenuHits;	// Player 1's START menu, as last drawn

static void GESS_AddMenuHit( int slot, int x0, int y0, int x1, int y1, int hit, int arg = 0 )
{
	if ( slot != 0 )
		return;
	GESSMenuHit_t &h = s_MenuHits[s_MenuHits.AddToTail()];
	h.x0 = x0; h.y0 = y0; h.x1 = x1; h.y1 = y1;
	h.hit = hit; h.arg = arg;
}

static const GESSMenuHit_t *GESS_FindMenuHit( int x, int y )
{
	for ( int i = s_MenuHits.Count() - 1; i >= 0; i-- )
	{
		const GESSMenuHit_t &h = s_MenuHits[i];
		if ( x >= h.x0 && x < h.x1 && y >= h.y0 && y < h.y1 )
			return &h;
	}
	return NULL;
}

class CGESSMenuMouse : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGESSMenuMouse, vgui::Panel );
public:
	CGESSMenuMouse() : BaseClass( NULL, "GESSMenuMouse" )
	{
		SetParent( g_pClientMode->GetViewport() );
		MakePopup( false, false );
		SetPaintBackgroundEnabled( false );
		SetPaintEnabled( false );
		SetKeyBoardInputEnabled( false );
		SetMouseInputEnabled( true );
		SetVisible( false );
	}

	virtual void OnMousePressed( vgui::MouseCode code )
	{
		if ( code == MOUSE_RIGHT )
		{
			GESplitScreenClient()->OnMenuMouseBack();
			return;
		}
		if ( code != MOUSE_LEFT )
			return;
		int x, y;
		vgui::input()->GetCursorPos( x, y );
		const GESSMenuHit_t *h = GESS_FindMenuHit( x, y );
		if ( h )
			GESplitScreenClient()->OnMenuMouse( h->hit, h->arg, true, Frac( h, x ) );
	}

	virtual void OnCursorMoved( int, int )
	{
		int x, y;
		vgui::input()->GetCursorPos( x, y );
		const GESSMenuHit_t *h = GESS_FindMenuHit( x, y );
		if ( h )
			GESplitScreenClient()->OnMenuMouse( h->hit, h->arg, false, Frac( h, x ) );
	}

private:
	// How far along the hit area (a slider) the cursor is, 0-1
	static float Frac( const GESSMenuHit_t *h, int x )
	{
		return h->x1 > h->x0 ? clamp( (float)( x - h->x0 ) / (float)( h->x1 - h->x0 ), 0.0f, 1.0f ) : 0.0f;
	}
};

static vgui::DHANDLE<CGESSMenuMouse> s_hMenuMouse;

// Called every frame by the split screen client: the popup shows (with the cursor)
// only while player 1's START menu wants the mouse
void GESS_UpdateMenuMouse( bool bWant )
{
	if ( !s_hMenuMouse.Get() )
	{
		if ( !bWant || !g_pClientMode || !g_pClientMode->GetViewport() )
			return;
		s_hMenuMouse = new CGESSMenuMouse();
	}
	CGESSMenuMouse *p = s_hMenuMouse.Get();
	if ( bWant )
	{
		int w, h;
		surface()->GetScreenSize( w, h );
		p->SetBounds( 0, 0, w, h );
	}
	if ( p->IsVisible() != bWant )
		p->SetVisible( bWant );
	if ( !bWant )
		s_MenuHits.RemoveAll();
}

// START during a match: leaderboard, swap team, exit / commit suicide (left / right)
void CGESplitScreenHud::PaintGameMenu( int slot, const GESSViewport_t &vp )
{
	CGESplitScreenClient *pSS = GESplitScreenClient();
	const int page = pSS->GetSlotMenuSelection( slot );
	const int tallL = surface()->GetFontTall( m_hFontLarge );
	const int tallS = surface()->GetFontTall( m_hFontSmall );
	const bool bMouse = slot == 0 && pSS->WantsMenuMouse();
	if ( slot == 0 )
		s_MenuHits.RemoveAll();

	// Dark background (no border) with the page name and arrows on top, help underneath
	const int inset = Max( 6, vp.h / 30 );
	GESSViewport_t box = { vp.x + inset, vp.y + inset, vp.w - inset * 2, vp.h - inset * 2 };
	// The leaderboard is GoldenEye's scoreboard over the game, as when holding TAB
	const bool bBoard = page == GESS_GAMEPAGE_LEADERBOARD;
	if ( !bBoard )
	{
		surface()->DrawSetColor( Color( 10, 10, 10, 225 ) );
		surface()->DrawFilledRect( box.x, box.y, box.x + box.w, box.y + box.h );
	}

	static const char *s_pszPages[GESS_GAMEPAGE_COUNT] = { "Leaderboard", "Swap team", "Controller", "Exit game" };
	const int cx = box.x + box.w / 2;
	int y = box.y + 6;

	// The leaderboard has no title (it sat on the round timer); with the mouse, < and >
	// halfway down its sides change page
	if ( bBoard && bMouse )
	{
		const int ay = vp.y + vp.h / 2 - tallL / 2, aw = TextWide( m_hFontLarge, ">" );
		DrawText( m_hFontLarge, vp.x + 8, ay, Color( 230, 200, 80, 220 ), "<" );
		DrawText( m_hFontLarge, vp.x + vp.w - 8 - aw, ay, Color( 230, 200, 80, 220 ), ">" );
		GESS_AddMenuHit( slot, vp.x, ay - tallL, vp.x + 16 + aw * 2, ay + tallL * 2, GESS_MENUHIT_PREV );
		GESS_AddMenuHit( slot, vp.x + vp.w - 16 - aw * 2, ay - tallL, vp.x + vp.w, ay + tallL * 2, GESS_MENUHIT_NEXT );
	}

	// The page name, arrows and page dots on the other pages (Calibrate Sticks: its own
	// title, no arrows: it's left with Done / Back)
	const bool bCal = page == GESS_GAMEPAGE_CONTROLLER && pSS->IsSlotCalibrateOpen( slot );
	if ( !bBoard )
	{
		DrawText( m_hFontLarge, cx, y, Color( 230, 200, 80, 255 ), bCal ? "Calibrate Sticks" : s_pszPages[clamp( page, 0, GESS_GAMEPAGE_COUNT - 1 )], true );
		if ( !bCal )
		{
			const int aw = TextWide( m_hFontLarge, ">" );
			// On the Controller page, the title row is highlighted while left / right change page
			const bool bTitleSel = page == GESS_GAMEPAGE_CONTROLLER && pSS->GetSlotCtrlRow( slot ) == GESS_CTRLROW_PAGE;
			const int a = bTitleSel || page != GESS_GAMEPAGE_CONTROLLER ? 200 : 90;
			DrawText( m_hFontLarge, box.x + 12, y, Color( 230, 200, 80, a ), "<" );
			DrawText( m_hFontLarge, box.x + box.w - 12 - aw, y, Color( 230, 200, 80, a ), ">" );
			GESS_AddMenuHit( slot, box.x, y - 4, box.x + 24 + aw * 2, y + tallL + 4, GESS_MENUHIT_PREV );
			GESS_AddMenuHit( slot, box.x + box.w - 24 - aw * 2, y - 4, box.x + box.w, y + tallL + 4, GESS_MENUHIT_NEXT );
		}
		y += tallL + 2;

		int numPages = 0;
		for ( int p = 0; p < GESS_GAMEPAGE_COUNT; p++ )
			numPages += pSS->IsGameMenuPageAvailable( p ) ? 1 : 0;
		const int dot = Max( 4, tallS / 3 ), dotGap = dot * 2;
		int dx = cx - ( numPages * dotGap ) / 2;
		for ( int p = 0; p < GESS_GAMEPAGE_COUNT; p++ )
		{
			if ( !pSS->IsGameMenuPageAvailable( p ) )
				continue;
			surface()->DrawSetColor( p == page ? Color( 230, 200, 80, 255 ) : Color( 110, 110, 110, 255 ) );
			surface()->DrawFilledRect( dx, y, dx + dot, y + dot );
			dx += dotGap;
		}
		y += dot + 6;
	}

	const int helpY = box.y + box.h - tallS - 6;
	GESSViewport_t content = { box.x + 8, y, box.w - 16, helpY - y - 6 };
	const char *help = "Left / Right: more    START / B: close";

	switch ( page )
	{
	case GESS_GAMEPAGE_LEADERBOARD:
	{
		// The player's own row is highlighted, as in the normal game
		C_BasePlayer *pViewer = pSS->GetSlotPlayer( slot );
		// Centred in the view and as big as it allows under the round timer and above
		// the help line (the radar is hidden meanwhile), so it can be read from the sofa
		// even in a quarter of the screen. The health and armour dials wrap around its
		// sides, so it leaves room for them: its half width, the gap and a dial must fit
		// in half the view.
		const int top = vp.y + Max( 4, vp.h / 40 ) + tallL + 4;
		const int bottom = helpY - 4;
		const int margin = Max( 4, vp.w / 60 );
		const int vcx = vp.x + vp.w / 2, vcy = vp.y + vp.h / 2;
		const int halfH = Max( 10, Min( vcy - top, bottom - vcy ) );
		const float dialW = GESS_DIAL_W * GESS_BOARD_VIS_H * GESS_BOARD_DIAL_TALL / GESS_DIAL_H;
		const float halfUnits = GESS_BOARD_VIS_W / 2 + GESS_BOARD_DIAL_GAP + dialW;
		const float bs = Min( ( vp.w / 2 - margin ) / halfUnits, ( halfH * 2 ) / GESS_BOARD_VIS_H );
		const int bw = (int)( GESS_BOARD_VIS_W * bs ), bh = (int)( GESS_BOARD_VIS_H * bs );
		GESSViewport_t board = { vcx - bw / 2, vcy - bh / 2, bw, bh };

		C_GEPlayer *pGEViewer = ToGEPlayer( pViewer );
		if ( pGEViewer && pGEViewer->IsAlive() && !pGEViewer->IsObserver() && pGEViewer->GetTeamNumber() != TEAM_SPECTATOR )
			PaintLifeDials( pGEViewer, vp, 1.0f, &board );
		PaintScoreboard( board, pViewer ? pViewer->entindex() : -1 );
		break;
	}

	case GESS_GAMEPAGE_TEAM:
	{
		C_BasePlayer *pPlayer = pSS->GetSlotPlayer( slot );
		const int team = pPlayer ? pPlayer->GetTeamNumber() : pSS->GetSlotTeam( slot );
		const bool bMI6 = team == TEAM_MI6;
		const int midY = content.y + content.h / 2;
		char line[96];
		Q_snprintf( line, sizeof( line ), "You are on %s", bMI6 ? "MI6" : "Janus" );
		DrawText( m_hFontLarge, cx, midY - tallL - tallS, bMI6 ? Color( 120, 170, 255, 255 ) : Color( 255, 110, 110, 255 ), line, true );
		Q_snprintf( line, sizeof( line ), "Press A / RIGHT TRIGGER to swap to %s", bMI6 ? "Janus" : "MI6" );
		DrawText( m_hFontSmall, cx, midY, Color( 255, 255, 255, 230 ), line, true );
		DrawText( m_hFontSmall, cx, midY + tallS + 6, Color( 180, 180, 180, 220 ), "(you respawn on the new team straight away)", true );
		GESS_AddMenuHit( slot, content.x, midY - 4, content.x + content.w, midY + tallS + 4, GESS_MENUHIT_TEAM );
		break;
	}

	case GESS_GAMEPAGE_CONTROLLER:
		help = bCal ? PaintCalibrate( slot, content ) : PaintControllerPage( slot, content );
		break;

	case GESS_GAMEPAGE_EXIT:
	{
		// Exit game, with Commit Suicide under it. Each asks "are you sure" first.
		const int midY = content.y + content.h / 2;
		const int confirm = pSS->GetSlotMenuConfirm( slot );
		const char *pszSuicide = GESS_SuicideLabel();
		const bool bCanSuicide = pSS->CanSlotSuicide( slot );
		const int bh = tallL + 8;
		const char *pszLabels[2];
		int sel, hit;
		bool bEnabled[2] = { true, true };
		if ( confirm == GESS_CONFIRM_NONE )
		{
			pszLabels[0] = "Exit game";
			pszLabels[1] = pszSuicide;
			bEnabled[1] = bCanSuicide;
			sel = pSS->GetSlotMenuSub( slot );
			hit = GESS_MENUHIT_BUTTON;
			help = "Up / Down: choose    A / RIGHT TRIGGER: select    Left / Right: more";
		}
		else
		{
			const char *pszQuestion = confirm == GESS_CONFIRM_SUICIDE ? "Are you sure?" :
				( slot == 0 ? "End the match for everyone?" : "Leave the game?" );
			DrawText( m_hFontLarge, cx, midY - tallL * 2 - 8, Color( 255, 255, 255, 255 ), pszQuestion, true );
			if ( confirm == GESS_CONFIRM_SUICIDE )
				DrawText( m_hFontSmall, cx, midY - tallL - 4, Color( 200, 200, 200, 220 ), pszSuicide, true );
			pszLabels[0] = "Cancel";
			pszLabels[1] = "Confirm";
			sel = pSS->GetSlotMenuConfirmSel( slot );
			hit = GESS_MENUHIT_CONFIRM;
			help = "Up / Down: choose    A / RIGHT TRIGGER: answer    B: cancel";
		}

		const int bw = Min( content.w, Max( content.w / 3, Max( TextWide( m_hFontLarge, pszLabels[0] ), TextWide( m_hFontLarge, pszLabels[1] ) ) + 30 ) );
		for ( int i = 0; i < 2; i++ )
		{
			const int by = midY - tallL / 2 + i * ( bh + 6 );
			const bool bSel = sel == i;
			surface()->DrawSetColor( bSel ? Color( 150, 125, 50, 255 ) : Color( 40, 40, 40, 255 ) );
			surface()->DrawFilledRect( cx - bw / 2, by, cx + bw / 2, by + bh );
			surface()->DrawSetColor( bSel ? Color( 255, 230, 120, 255 ) : Color( 90, 90, 90, 255 ) );
			surface()->DrawOutlinedRect( cx - bw / 2, by, cx + bw / 2, by + bh );
			DrawText( m_hFontLarge, cx, by + 4, bEnabled[i] ? Color( 255, 255, 255, 255 ) : Color( 130, 130, 130, 255 ), pszLabels[i], true );
			GESS_AddMenuHit( slot, cx - bw / 2, by, cx + bw / 2, by + bh, hit, i );
		}
		if ( confirm == GESS_CONFIRM_EXIT && slot == 0 )
			DrawText( m_hFontSmall, cx, midY + bh * 2 + 18, Color( 200, 200, 200, 220 ), "The results are shown, then back to the menu", true );
		else if ( confirm == GESS_CONFIRM_NONE && !bCanSuicide )
			DrawText( m_hFontSmall, cx, midY + bh * 2 + 18, Color( 200, 200, 200, 220 ), "(only while you're alive)", true );
		break;
	}
	}

	DrawText( m_hFontSmall, cx, helpY, Color( 200, 200, 200, 230 ), help, true );
}

// The dead zone a stick has before it's measured (ges_ss_pad_deadzone)
static float GESS_FixedDeadzone()
{
	static ConVarRef ges_ss_pad_deadzone( "ges_ss_pad_deadzone" );
	return ges_ss_pad_deadzone.IsValid() ? ges_ss_pad_deadzone.GetFloat() : 0.22f;
}

void CGESplitScreenHud::PaintMenuRowBack( int x, int y, int w, int h, bool bSel )
{
	if ( !bSel )
		return;
	surface()->DrawSetColor( Color( 230, 200, 80, 70 ) );
	surface()->DrawFilledRect( x, y, x + w, y + h );
}

void CGESplitScreenHud::PaintSlider( int slot, int row, int x, int y, int w, int h, float frac, const char *value, bool bSel )
{
	// The track, filled up to the value (no border), and the value after it
	const int vw = TextWide( m_hFontSmall, "0.00" ) + 8;
	const int tw = Max( 10, w - vw ), th = Max( 4, h / 3 ), ty = y + ( h - th ) / 2;
	surface()->DrawSetColor( Color( 60, 60, 60, 255 ) );
	surface()->DrawFilledRect( x, ty, x + tw, ty + th );
	surface()->DrawSetColor( bSel ? Color( 255, 220, 100, 255 ) : Color( 185, 160, 70, 255 ) );
	surface()->DrawFilledRect( x, ty, x + (int)( tw * clamp( frac, 0.0f, 1.0f ) ), ty + th );
	DrawText( m_hFontSmall, x + tw + 6, y + ( h - surface()->GetFontTall( m_hFontSmall ) ) / 2, Color( 255, 255, 255, 255 ), value );
	GESS_AddMenuHit( slot, x, y, x + tw, y + h, GESS_MENUHIT_SLIDER, row );
}

const char *CGESplitScreenHud::PaintControllerPage( int slot, const GESSViewport_t &area )
{
	CGESplitScreenClient *pSS = GESplitScreenClient();
	const int sel = pSS->GetSlotCtrlRow( slot );
	// Large text when the three rows fit, small in a small view
	const HFont font = ( surface()->GetFontTall( m_hFontLarge ) + 10 ) * 3 <= area.h ? m_hFontLarge : m_hFontSmall;
	const int tall = surface()->GetFontTall( font ), rowH = tall + 10;
	int y = area.y + Max( 0, ( area.h - rowH * 3 ) / 2 );
	const int lx = area.x + 10, rx = area.x + area.w - 10;

	for ( int row = GESS_CTRLROW_INVERT; row < GESS_CTRLROW_COUNT; row++, y += rowH )
	{
		const bool bSel = sel == row;
		PaintMenuRowBack( area.x, y, area.w, rowH - 2, bSel );
		GESS_AddMenuHit( slot, area.x, y, area.x + area.w, y + rowH - 2, GESS_MENUHIT_ROW, row );
		const int ty = y + ( rowH - 2 - tall ) / 2;
		const Color col = bSel ? Color( 255, 255, 255, 255 ) : Color( 210, 210, 210, 230 );
		switch ( row )
		{
		case GESS_CTRLROW_INVERT:
		{
			DrawText( font, lx, ty, col, "Invert Look Up/Down" );
			const char *v = pSS->GetSlotInvert( slot ) ? "On" : "Off";
			DrawText( font, rx - TextWide( font, v ), ty, Color( 255, 220, 100, 255 ), v );
			break;
		}
		case GESS_CTRLROW_SENS:
		{
			DrawText( font, lx, ty, col, "Look Sensitivity" );
			const int step = pSS->GetSlotSensStep( slot );
			char v[8];
			Q_snprintf( v, sizeof( v ), "%d", step );
			const int sx = Max( lx + TextWide( font, "Look Sensitivity" ) + 12, area.x + area.w / 2 );
			PaintSlider( slot, row, sx, y + 2, rx - sx, rowH - 6, (float)( step - 1 ) / (float)( GESS_SENS_STEPS - 1 ), v, bSel );
			break;
		}
		case GESS_CTRLROW_CALIBRATE:
			DrawText( font, lx, ty, col, "Calibrate Sticks" );
			DrawText( font, rx - TextWide( font, "..." ), ty, Color( 255, 220, 100, 255 ), "..." );
			break;
		}
	}

	if ( sel == GESS_CTRLROW_PAGE )
		return "Left / Right: more    Down: settings    START / B: close";
	return "Up / Down: choose    Left / Right: change    A: select    B: close";
}

const char *CGESplitScreenHud::PaintCalibrate( int slot, const GESSViewport_t &area )
{
	CGESplitScreenClient *pSS = GESplitScreenClient();
	CGESSGamepads *pPads = GESSGamepads();
	const int pad = pSS->GetSlotPad( slot );
	const int sel = pSS->GetSlotCalRow( slot );
	const int tallS = surface()->GetFontTall( m_hFontSmall );
	const int rowH = tallS + 6;
	const int rowsH = rowH * GESS_CALROW_COUNT;

	// The two sticks, live: the circle is the stick's full range, the gold circle its
	// dead zone (around where it rests), the dot where it is (green outside the dead zone)
	const int stickH = Max( 0, area.h - rowsH - tallS * 2 - 12 );
	const int R = Max( 6, Min( ( stickH - tallS - 4 ) / 2, area.w / 4 - 12 ) );
	for ( int st = 0; st < 2; st++ )
	{
		const CGESSGamepads::StickCal_t &sc = pPads->GetStickCal( pad, st );
		const float inner = sc.measured ? sc.inner : GESS_FixedDeadzone();
		const int cx = area.x + ( st == 0 ? area.w / 4 : area.w * 3 / 4 ), cy = area.y + R + 2;
		surface()->DrawSetColor( Color( 120, 120, 120, 255 ) );
		surface()->DrawOutlinedCircle( cx, cy, R, 48 );
		const int dzx = cx + (int)( sc.cx * R ), dzy = cy - (int)( sc.cy * R );
		surface()->DrawSetColor( Color( 230, 200, 80, 255 ) );
		surface()->DrawOutlinedCircle( dzx, dzy, Max( 1, (int)( inner * R ) ), 32 );
		float x, y;
		pPads->GetRawStick( pad, st, x, y );
		const float dx = x - sc.cx, dy = y - sc.cy;
		const bool bOut = dx * dx + dy * dy > inner * inner;
		const int px = cx + (int)( clamp( x, -1.2f, 1.2f ) * R ), py = cy - (int)( clamp( y, -1.2f, 1.2f ) * R );
		const int d = Max( 2, R / 12 );
		surface()->DrawSetColor( bOut ? Color( 90, 230, 90, 255 ) : Color( 255, 255, 255, 255 ) );
		surface()->DrawFilledRect( px - d, py - d, px + d + 1, py + d + 1 );
		char label[48];
		Q_snprintf( label, sizeof( label ), "%s stick  %.2f", st == 0 ? "Left" : "Right", inner );
		DrawText( m_hFontSmall, cx, cy + R + 2, Color( 220, 220, 220, 255 ), label, true );
	}

	// How the measurement is going (or the R3 hint)
	int y = area.y + R * 2 + tallS + 8;
	char prompt[128];
	DrawText( m_hFontSmall, area.x + area.w / 2, y, Color( 255, 220, 100, 255 ), pPads->GetCalibrationPrompt( pad, prompt, sizeof( prompt ) ), true );
	y += tallS + 6;

	const int lx = area.x + 10, rx = area.x + area.w - 10;
	const float lo = CGESSGamepads::GetDeadzoneMin(), hi = CGESSGamepads::GetDeadzoneMax();
	static const char *s_pszRows[GESS_CALROW_COUNT] = { "Re-measure (R3)", "Left dead zone", "Right dead zone", "Done", "Back" };
	for ( int row = 0; row < GESS_CALROW_COUNT; row++, y += rowH )
	{
		const bool bSel = sel == row;
		PaintMenuRowBack( area.x, y, area.w, rowH - 1, bSel );
		GESS_AddMenuHit( slot, area.x, y, area.x + area.w, y + rowH - 1, GESS_MENUHIT_ROW, row );
		DrawText( m_hFontSmall, lx, y + 3, bSel ? Color( 255, 255, 255, 255 ) : Color( 210, 210, 210, 230 ), s_pszRows[row] );
		if ( row == GESS_CALROW_LEFT || row == GESS_CALROW_RIGHT )
		{
			const CGESSGamepads::StickCal_t &sc = pPads->GetStickCal( pad, row == GESS_CALROW_LEFT ? 0 : 1 );
			const float inner = sc.measured ? sc.inner : GESS_FixedDeadzone();
			char v[16];
			Q_snprintf( v, sizeof( v ), "%.2f", inner );
			const int sx = Max( lx + TextWide( m_hFontSmall, "Right dead zone" ) + 12, area.x + area.w / 2 );
			PaintSlider( slot, row, sx, y + 1, rx - sx, rowH - 2, ( inner - lo ) / Max( 0.01f, hi - lo ), v, bSel );
		}
	}

	return "Up / Down: choose    Left / Right: dead zone    R3: re-measure    A: select    B: back";
}

void CGESplitScreenHud::PaintMenu( int slot, const GESSViewport_t &vp )
{
	CGESplitScreenClient *pSS = GESplitScreenClient();
	const GESSSlotMenu_t menu = pSS->GetSlotMenu( slot );
	const int sel = pSS->GetSlotMenuSelection( slot );

	if ( menu == GESS_MENU_PAUSE )
	{
		PaintGameMenu( slot, vp );
		return;
	}

	int count;
	if ( menu == GESS_MENU_PROFILE )
		count = pSS->GetMenuProfileCount();
	else if ( menu == GESS_MENU_TEAM )
		count = GESS_TEAMMENU_COUNT;
	else
		count = slot == 0 ? pSS->GetPauseMenuCount() - 1 : pSS->GetPauseMenuCount();
	const int lineTall = surface()->GetFontTall( m_hFontSmall ) + 4;
	const int titleTall = surface()->GetFontTall( m_hFontLarge ) + 6;

	// Show at most what fits in the viewport, scrolled to the selection
	const int maxLines = Max( 1, ( vp.h * 3 / 4 - titleTall - lineTall * 2 ) / lineTall );
	const int first = clamp( sel - maxLines / 2, 0, Max( 0, count - maxLines ) );
	const int lines = Min( count, maxLines );

	const int boxW = Min( vp.w - 16, Max( vp.w * 2 / 3, 220 ) );
	const int boxH = titleTall + lines * lineTall + lineTall + 12;
	const int boxX = vp.x + ( vp.w - boxW ) / 2;
	const int boxY = vp.y + ( vp.h - boxH ) / 2;

	surface()->DrawSetColor( Color( 10, 10, 10, 220 ) );
	surface()->DrawFilledRect( boxX, boxY, boxX + boxW, boxY + boxH );
	surface()->DrawSetColor( Color( 230, 200, 80, 255 ) );
	surface()->DrawOutlinedRect( boxX, boxY, boxX + boxW, boxY + boxH );

	char title[64];
	if ( menu == GESS_MENU_PROFILE )
		Q_snprintf( title, sizeof( title ), "Player %d - choose profile", slot + 1 );
	else if ( menu == GESS_MENU_TEAM )
		Q_snprintf( title, sizeof( title ), "Player %d - choose team", slot + 1 );
	else
		Q_snprintf( title, sizeof( title ), "Player %d", slot + 1 );
	DrawText( m_hFontLarge, boxX + boxW / 2, boxY + 4, Color( 230, 200, 80, 255 ), title, true );

	int y = boxY + titleTall;
	for ( int i = first; i < first + lines; i++ )
	{
		char label[96];
		bool bDisabled = false;

		if ( menu == GESS_MENU_PROFILE )
		{
			const int idx = pSS->GetMenuProfileIndex( i );
			const GESSProfile_t *p = GESSProfiles()->Get( idx );
			if ( p )
			{
				const CGECharData *pChar = GECharacters()->Get( p->character );
				// Character names are localization tokens (#GE_BOND_CONCISE)
				char szChar[64];
				Q_strncpy( szChar, pChar ? pChar->szShortName : "Random", sizeof( szChar ) );
				const wchar_t *pwszChar = pChar ? g_pVGuiLocalize->Find( pChar->szShortName ) : NULL;
				if ( pwszChar )
					g_pVGuiLocalize->ConvertUnicodeToANSI( pwszChar, szChar, sizeof( szChar ) );
				Q_snprintf( label, sizeof( label ), "%s  (%s)", p->name, szChar );
				bDisabled = pSS->IsSlotProfileInUse( idx, slot );
				if ( bDisabled )
					Q_strncat( label, " - in use", sizeof( label ), COPY_ALL_CHARACTERS );
			}
			else
			{
				Q_strncpy( label, "Guest", sizeof( label ) );
			}
		}
		else if ( menu == GESS_MENU_TEAM )
		{
			Q_strncpy( label, pSS->GetTeamMenuLabel( i ), sizeof( label ) );
		}
		else
		{
			Q_strncpy( label, pSS->GetPauseMenuLabel( i ), sizeof( label ) );
		}

		if ( i == sel )
		{
			surface()->DrawSetColor( Color( 230, 200, 80, 90 ) );
			surface()->DrawFilledRect( boxX + 4, y - 1, boxX + boxW - 4, y + lineTall - 3 );
		}

		DrawText( m_hFontSmall, boxX + 12, y, bDisabled ? Color( 140, 140, 140, 255 ) : Color( 255, 255, 255, 255 ), label );
		y += lineTall;
	}

	DrawText( m_hFontSmall, boxX + boxW / 2, boxY + boxH - lineTall - 2, Color( 200, 200, 200, 200 ), "A: select   B: back", true );
}
