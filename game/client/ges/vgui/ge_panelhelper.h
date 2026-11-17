///////////// Copyright © 2010 LodleNet. All rights reserved. /////////////
//
//   Project     : Client
//   File        : ge_panelhelper.h
//   Description :
//      [TODO: Write the purpose of ge_panelhelper.h.]
//
//   Created On: 2/27/2010 4:06:23 PM
//   Created By: Mark Chandler <mailto:mark@moddb.com>
////////////////////////////////////////////////////////////////////////////

#ifndef MC_GE_PANELHELPER_H
#define MC_GE_PANELHELPER_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>

class IGEGameUI
{
public:
	virtual void Create( vgui::VPANEL parent ) = 0;
	virtual void Destroy( void ) = 0;
	virtual vgui::Panel *GetPanel(void) = 0;
};


template <class T>
class GameUI : public IGEGameUI
{
public:
	GameUI()
	{
		m_pPanel = NULL;
		// Register us internally to be created at VGui_CreateGlobalPanels()
		registerGEPanel(this);
	}

	void Create(vgui::VPANEL parent)
	{
		m_pPanel = new T(parent);
	}

	void Destroy( void )
	{
		if (m_pPanel)
		{
			m_pPanel->SetParent( (vgui::Panel*)NULL );
			delete m_pPanel;
		}

		m_pPanel = NULL;
	}

	T* operator->() const
	{
		return m_pPanel;
	}

	T* GetNativePanel()
	{
		return m_pPanel;
	}

	vgui::Panel *GetPanel(void)
	{
		return GetNativePanel();
	}

private:
	T* m_pPanel;
};

#define CenterThisPanelOnScreen() CenterPanelOnScreen(this);

inline void CenterPanelOnScreen(vgui::Panel* panel)
{
	if (!panel)
		return;

	int x,w,h;

	panel->GetBounds(x,x,w,h);
	panel->SetPos((ScreenWidth()-w)/2,(ScreenHeight()-h)/2);
}

inline bool IsWidescreen()
{
	if ( ((float) ScreenWidth() / (float) ScreenHeight()) > 1.4f )
		return true;
	else
		return false;
}

//-----------------------------------------------------------------------------
// Menu sizes. One factor for 1080p, GE_MENU_SCALE_1080; 1440p and 4K grow from
// it (x1.5 and x2.5 of it: a bit more than the screen height alone, x1.33 / x2).
// Below 1024 lines nothing changes. The fonts follow the same steps (yres ranges
// 1024-1199, 1200-1599, 1600+) in the schemes made by
// splitscreen_resources/tools/make_menu_schemes.pl: change $SCALE_1080 there too
// and run it again when changing GE_MENU_SCALE_1080.
//  - Panels laid out in 1080p pixels (Split Screen menu, Create Server) use
//    GESMenuScheme.res and GEMenuScale()
//  - Proportional panels (credits, update checker, intro video) already grow with
//    the screen height (h / 480): GEMenuPropScale() is the extra on top, and
//    their fonts come from GESPropScheme.res
//  - GameUI (SourceScheme.res): the main menu list's font grows like the panels;
//    GameUI's own dialogs are fixed size, so their fonts only grow to 16 pixels
//-----------------------------------------------------------------------------
#define GE_MENU_SCALE_1080	1.5f

float GEMenuScale( void );
float GEMenuPropScale( void );
inline int GEMenuScaled( int v ) { const float f = v * GEMenuScale(); return (int)( f >= 0.0f ? f + 0.5f : f - 0.5f ); }

// Grows a panel's children (as LoadControlSettings placed them) by factor. With
// bFromEdges each child keeps its distance (times factor) to the nearest edge or
// the centre, for full screen panels; otherwise everything grows from the top
// left, for a frame that grows by the same factor (GEMenuZoomFrame).
void GEMenuZoomChildren( vgui::Panel *pPanel, float factor, bool bFromEdges );
// Grows a frame by factor around its centre, then its children
void GEMenuZoomFrame( vgui::Panel *pPanel, float factor );

// Frame's close (and minimise...) buttons are a fixed 18 pixels: lays them out
// factor times bigger, from the frame's PerformLayout after Frame's own
void GEMenuLayoutFrameButtons( vgui::Panel *pFrame, float factor );

void StopMenuMusic( void );
void StartMenuMusic( void );

bool IsInGame( void );

void registerGEPanel( IGEGameUI* panel );
void createGEPanels( vgui::VPANEL parent );
void destroyGEPanels();


#endif //MC_GE_PANELHELPER_H
