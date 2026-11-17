///////////// Copyright © 2010 LodleNet. All rights reserved. /////////////
//
//   Project     : Client
//   File        : ge_panelhelper.cpp
//   Description :
//      [TODO: Write the purpose of ge_panelhelper.cpp.]
//
//   Created On: 2/27/2010 4:07:20 PM
//   Created By: Mark Chandler <mailto:mark@moddb.com>
////////////////////////////////////////////////////////////////////////////

#include "cbase.h"
#include "ge_panelhelper.h"
#include <vgui/ISurface.h>
#include "ge_gamerules.h"
#include "clientmode_ge.h"
#include "ge_musicmanager.h"
#include "ge_loadingscreen.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Menu sizes (see ge_panelhelper.h); the steps match the yres ranges of the
// font sizes in GESMenuScheme.res and GESPropScheme.res
//-----------------------------------------------------------------------------
static int GEMenuScreenTall( void )
{
	int w, h;
	vgui::surface()->GetScreenSize( w, h );
	return h;
}

float GEMenuScale( void )
{
	const int h = GEMenuScreenTall();
	if ( h < 1024 )
		return 1.0f;
	return GE_MENU_SCALE_1080 * ( h >= 1600 ? 2.5f : ( h >= 1200 ? 1.5f : 1.0f ) );
}

// Proportional panels are already h / 480: 1.125 x 1440/1080 = 1.5, 1.25 x 2160/1080 = 2.5
float GEMenuPropScale( void )
{
	const int h = GEMenuScreenTall();
	if ( h < 1024 )
		return 1.0f;
	return GE_MENU_SCALE_1080 * ( h >= 1600 ? 1.25f : ( h >= 1200 ? 1.125f : 1.0f ) );
}

static int GEMenuRound( float f )
{
	return (int)( f >= 0.0f ? f + 0.5f : f - 0.5f );
}

// Scales a span [p, p+len) inside [0, total): from the start, the end or the centre,
// whichever the span is nearest to
static void GEMenuZoomSpan( int &p, int &len, int total, float factor, bool bFromEdges )
{
	const int newLen = GEMenuRound( len * factor );
	if ( !bFromEdges )
	{
		p = GEMenuRound( p * factor );
	}
	else
	{
		const int mid = p + len / 2;
		if ( mid > total * 2 / 3 )
			p = total - GEMenuRound( ( total - p - len ) * factor ) - newLen;	// keeps its distance to the far edge
		else if ( mid >= total / 3 )
			p = total / 2 + GEMenuRound( ( p + len / 2 - total / 2 ) * factor ) - newLen / 2;	// to the centre
		else
			p = GEMenuRound( p * factor );
	}
	len = newLen;
}

void GEMenuZoomChildren( vgui::Panel *pPanel, float factor, bool bFromEdges )
{
	if ( !pPanel || factor == 1.0f )
		return;

	const int W = pPanel->GetWide(), H = pPanel->GetTall();
	for ( int i = 0; i < pPanel->GetChildCount(); i++ )
	{
		vgui::Panel *pChild = pPanel->GetChild( i );
		// The frame's own grips and buttons are placed by Frame::PerformLayout
		if ( !pChild || !Q_strncmp( pChild->GetName(), "frame_", 6 ) )
			continue;

		int x, y, w, h;
		pChild->GetBounds( x, y, w, h );
		GEMenuZoomSpan( x, w, W, factor, bFromEdges );
		GEMenuZoomSpan( y, h, H, factor, bFromEdges );
		pChild->SetBounds( x, y, w, h );

		// EditablePanel::OnSizeChanged puts children back at their pin offsets
		// whenever the panel is resized, so the offsets have to match too
		const vgui::Panel::PinCorner_e pin = pChild->GetPinCorner();
		const vgui::Panel::AutoResize_e resize = pChild->GetAutoResize();
		if ( pin > vgui::Panel::PIN_BOTTOMRIGHT )
			continue;	// pinned to a sibling
		const bool bRight = pin == vgui::Panel::PIN_TOPRIGHT || pin == vgui::Panel::PIN_BOTTOMRIGHT;
		const bool bBottom = pin == vgui::Panel::PIN_BOTTOMLEFT || pin == vgui::Panel::PIN_BOTTOMRIGHT;
		const bool bResizeH = resize == vgui::Panel::AUTORESIZE_RIGHT || resize == vgui::Panel::AUTORESIZE_DOWNANDRIGHT;
		const bool bResizeV = resize == vgui::Panel::AUTORESIZE_DOWN || resize == vgui::Panel::AUTORESIZE_DOWNANDRIGHT;
		const int px = bRight ? x + w - W : x;
		const int py = bBottom ? y + h - H : y;
		const int ox = bResizeH ? ( bRight ? x : x + w - W ) : 0;
		const int oy = bResizeV ? ( bBottom ? y : y + h - H ) : 0;
		pChild->SetAutoResize( pin, resize, px, py, ox, oy );
	}
}

void GEMenuZoomFrame( vgui::Panel *pPanel, float factor )
{
	if ( !pPanel || factor == 1.0f )
		return;

	int x, y, w, h;
	pPanel->GetBounds( x, y, w, h );
	const int newW = GEMenuRound( w * factor ), newH = GEMenuRound( h * factor );
	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	x = clamp( x + ( w - newW ) / 2, 0, Max( 0, sw - newW ) );
	y = clamp( y + ( h - newH ) / 2, 0, Max( 0, sh - newH ) );
	pPanel->SetBounds( x, y, newW, newH );	// first: resizing puts the children back at their pin offsets
	GEMenuZoomChildren( pPanel, factor, false );
}

void GEMenuLayoutFrameButtons( vgui::Panel *pFrame, float factor )
{
	if ( !pFrame || factor == 1.0f )
		return;

	// Same places as Frame::PerformLayout, right to left
	static const char *s_pszButtons[] = { "frame_close", "frame_mintosystray", "frame_maximize", "frame_minimize" };
	const int side = GEMenuRound( 18 * factor ), step = GEMenuRound( 20 * factor );
	const int top = GEMenuRound( 8 * factor ), edge = GEMenuRound( 5 * factor );
	int offset = step;
	for ( int i = 0; i < ARRAYSIZE( s_pszButtons ); i++ )
	{
		vgui::Panel *pButton = pFrame->FindChildByName( s_pszButtons[i] );
		if ( !pButton || !pButton->IsVisible() )
			continue;
		pButton->SetBounds( pFrame->GetWide() - edge - offset, top, side, side );
		offset += step;
	}
}

// Helper functions to start and stop the menu music
void StopMenuMusic( void )
{
	if ( GEMusicManager() )
		GEMusicManager()->PauseMusic();
}

void StartMenuMusic( void )
{
	if ( GEMusicManager() )
	{
		// Reload our play list if we changed it (and are NOT in game)
		if ( !IsInGame() && Q_stricmp( GE_MUSIC_MENU, GEMusicManager()->CurrentPlaylist() ) )
		{
			GEMusicManager()->LoadPlayList( GE_MUSIC_MENU );
		}
		else
		{
			GEMusicManager()->ResumeMusic();
		}
	}

	if ( GetLoadingScreen() )
		GetLoadingScreen()->Reset();
}

bool IsInGame( void )
{
	// Since detecting g_pGameRules is unreliable we look to the world entity
	return g_pEntityList->LookupEntityByNetworkIndex(0) != NULL;
}

CUtlVector<IGEGameUI*> *g_pvPanelList = NULL;

// Called from our children who want to be part of the PANEL_GAMEUIDLL family
void registerGEPanel( IGEGameUI* panel )
{
	if ( !g_pvPanelList )
		g_pvPanelList = new CUtlVector<IGEGameUI*>();

	g_pvPanelList->AddToTail(panel);
}

// Called in vgui_int.cpp, VGui_CreateGlobalPanels(), the parent is PANEL_GAMEUIDLL
void createGEPanels( vgui::VPANEL parent )
{
	if ( !g_pvPanelList )
		return;

	for ( int x=0; x < g_pvPanelList->Count(); x++ )
	{
		g_pvPanelList->Element(x)->Create(parent);
	}
}

// Called in vgui_int.cpp, VGui_Shutdown()
void destroyGEPanels()
{
	if ( !g_pvPanelList )
		return;

	for ( int x=0; x < g_pvPanelList->Count(); x++ )
	{
		if ( g_pvPanelList->Element(x)->GetPanel() )
		{
			g_pvPanelList->Element(x)->Destroy();
		}
	}
}