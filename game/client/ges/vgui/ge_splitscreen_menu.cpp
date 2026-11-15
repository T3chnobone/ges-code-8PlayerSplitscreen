///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_splitscreen_menu.cpp
//   Description :
//      The Split Screen menu, opened from the main menu. Two pages:
//        * Profiles: name, character, controller sensitivity, inverted look
//          and whether the profile is invited. Also shows the controllers
//          and who is ready to join ("lobby").
//        * Match: map, game mode, teams and how the match is won (timed
//          score, first to N kills or best kill/death ratio), then Start.
//
//      The whole menu works with a controller: the d-pad moves between
//      settings, left/right changes them, A selects, B closes, Y switches
//      pages. Text is typed with the on-screen keyboard.
//
//      Open with ges_splitscreen, e.g. from resource/GameMenu.res:
//          "SplitScreen" { "label" "Split Screen" "command" "engine ges_splitscreen" }
//
////////////////////////////////////////////////////////////////////////////

#include "cbase.h"
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/IInput.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui/IImage.h>
#include "KeyValues.h"
#include "ienginevgui.h"

#include "ge_panelhelper.h"
#include "ge_character_data.h"
#include "ge_splitscreen_profiles.h"
#include "ge_splitscreen_client.h"
#include "ge_splitscreen_gamepad.h"
#include "ge_splitscreen_match.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define GES_SS_SENS_SLIDER_SCALE 100.0f

static const char *s_WinConditionNames[] =
{
	"Timed - highest score wins",
	"First to a number of kills",
	"Timed - best kill/death ratio wins",
};

// Controller buttons the menu listens to
static const unsigned int GESS_MENU_DIR_BITS[4] =
{
	GESS_PADBIT( GESS_PAD_DPAD_UP ), GESS_PADBIT( GESS_PAD_DPAD_DOWN ),
	GESS_PADBIT( GESS_PAD_DPAD_LEFT ), GESS_PADBIT( GESS_PAD_DPAD_RIGHT ),
};
enum { GESS_DIR_UP = 0, GESS_DIR_DOWN, GESS_DIR_LEFT, GESS_DIR_RIGHT };

// Reads every controller and turns it into menu presses, with key repeat
// for held d-pad directions. Edges are tracked here so a menu never misses
// or doubles a press, whatever order the game systems poll in.
class CGESSMenuPadReader
{
public:
	CGESSMenuPadReader() { Reset(); }

	void Reset()
	{
		memset( m_Prev, 0, sizeof( m_Prev ) );
		memset( m_bOwned, 0, sizeof( m_bOwned ) );
		memset( m_flRepeat, 0, sizeof( m_flRepeat ) );
		memset( m_flHeldSince, 0, sizeof( m_flHeldSince ) );
		m_Pressed = 0;
		m_bFirst = true;
	}

	void Update()
	{
		unsigned int held = 0, pressed = 0;
		const int hostPad = GESplitScreenClient()->GetSlotPad( 0 );
		for ( int i = 0; i < GESSGamepads()->GetMaxPads(); i++ )
		{
			const GESSPadState_t &pad = GESSGamepads()->GetPad( i );
			unsigned int b = pad.connected ? pad.buttons : 0;

			// Only player 1's controller drives the menus (the others only join /
			// leave and change their profile, in ge_splitscreen_client.cpp), and only
			// from the frame after it was claimed (the press that claims player 1
			// doesn't also select something)
			const bool bOwned = i == hostPad && GESplitScreenClient()->GetPadSlot( i ) == 0;
			const bool bWasOwned = m_bOwned[i];
			m_bOwned[i] = bOwned;
			if ( !bOwned || !bWasOwned )
			{
				m_Prev[i] = b;
				continue;
			}

			// Controllers other than player 1's can use the left stick as a d-pad
			// (player 1's sticks move the mouse cursor)
			if ( i != hostPad && pad.connected )
			{
				if ( pad.ly > 0.6f )	b |= GESS_PADBIT( GESS_PAD_DPAD_UP );
				if ( pad.ly < -0.6f )	b |= GESS_PADBIT( GESS_PAD_DPAD_DOWN );
				if ( pad.lx < -0.6f )	b |= GESS_PADBIT( GESS_PAD_DPAD_LEFT );
				if ( pad.lx > 0.6f )	b |= GESS_PADBIT( GESS_PAD_DPAD_RIGHT );
			}

			pressed |= b & ~m_Prev[i];
			held |= b;
			m_Prev[i] = b;
		}

		// Ignore buttons that were already held when the menu opened
		if ( m_bFirst )
		{
			m_bFirst = false;
			pressed = 0;
		}

		// Held directions repeat, getting faster the longer they're held
		const float now = gpGlobals->realtime;
		for ( int d = 0; d < 4; d++ )
		{
			const unsigned int bit = GESS_MENU_DIR_BITS[d];
			if ( pressed & bit )
			{
				m_flHeldSince[d] = now;
				m_flRepeat[d] = now + 0.4f;
			}
			else if ( ( held & bit ) && now >= m_flRepeat[d] )
			{
				pressed |= bit;
				m_flRepeat[d] = now + ( now - m_flHeldSince[d] > 1.5f ? 0.04f : 0.1f );
			}
		}

		m_Pressed = pressed;
	}

	bool Pressed( GESSPadButton_t b ) const	{ return ( m_Pressed & GESS_PADBIT( b ) ) != 0; }
	bool Dir( int d ) const					{ return ( m_Pressed & GESS_MENU_DIR_BITS[d] ) != 0; }
	bool Any() const						{ return m_Pressed != 0; }

private:
	unsigned int m_Prev[GES_SS_MAX_PADS];
	bool m_bOwned[GES_SS_MAX_PADS];
	unsigned int m_Pressed;
	float m_flRepeat[4];
	float m_flHeldSince[4];
	bool m_bFirst;
};

//-----------------------------------------------------------------------------
// On-screen keyboard for typing with a controller (or the mouse)
//-----------------------------------------------------------------------------
#define GESS_OSK_COLS 10
#define GESS_OSK_ROWS 5		// 4 rows of characters and a row of Shift / Space / Delete / Done

static const char *s_OSKLower[4] = { "1234567890", "qwertyuiop", "asdfghjkl'", "zxcvbnm-_." };
static const char *s_OSKUpper[4] = { "!?#$%&*()+", "QWERTYUIOP", "ASDFGHJKL'", "ZXCVBNM-_." };

enum { GESS_OSK_SHIFT = 0, GESS_OSK_SPACE, GESS_OSK_DELETE, GESS_OSK_DONE, GESS_OSK_SPECIAL_COUNT };
static const char *s_OSKSpecialNames[GESS_OSK_SPECIAL_COUNT] = { "Shift", "Space", "Delete", "Done" };
// The columns each special key covers on the bottom row
static const int s_OSKSpecialCol[GESS_OSK_SPECIAL_COUNT + 1] = { 0, 2, 6, 8, 10 };

class CGESSOnScreenKeyboard : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGESSOnScreenKeyboard, vgui::Panel );

public:
	CGESSOnScreenKeyboard( Panel *pParent ) : BaseClass( pParent, "GESSOnScreenKeyboard" )
	{
		m_pTarget = NULL;
		m_szText[0] = m_szOriginal[0] = m_szTitle[0] = 0;
		m_iMaxLen = 31;
		m_iRow = 1;
		m_iCol = 0;
		m_bShift = true;
		m_hFont = INVALID_FONT;
		SetVisible( false );
		SetMouseInputEnabled( true );
		SetKeyBoardInputEnabled( true );
		SetPaintBackgroundEnabled( false );
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		m_hFont = pScheme->GetFont( "DefaultLarge", false );
		if ( m_hFont == INVALID_FONT )
			m_hFont = pScheme->GetFont( "Default", false );
		m_hSmallFont = pScheme->GetFont( "Default", false );
	}

	void Open( TextEntry *pTarget, const char *title, int maxLen )
	{
		m_pTarget = pTarget;
		m_iMaxLen = clamp( maxLen, 1, (int) sizeof( m_szText ) - 1 );
		pTarget->GetText( m_szText, sizeof( m_szText ) );
		m_szText[m_iMaxLen] = 0;
		Q_strncpy( m_szOriginal, m_szText, sizeof( m_szOriginal ) );
		Q_strncpy( m_szTitle, title, sizeof( m_szTitle ) );
		m_bShift = m_szText[0] == 0;
		m_iRow = 1;
		m_iCol = 0;
		m_Pads.Reset();
		SetVisible( true );
		MoveToFront();
		RequestFocus();
	}

	// vgui::Panel::IsVisible() is not const in the SDK
	bool IsOpen() const { return const_cast<CGESSOnScreenKeyboard *>( this )->IsVisible(); }

	// Controller input, called by the menu every frame while open
	void HandlePads( const CGESSMenuPadReader &pads )
	{
		if ( pads.Dir( GESS_DIR_UP ) )		Move( 0, -1 );
		if ( pads.Dir( GESS_DIR_DOWN ) )	Move( 0, 1 );
		if ( pads.Dir( GESS_DIR_LEFT ) )	Move( -1, 0 );
		if ( pads.Dir( GESS_DIR_RIGHT ) )	Move( 1, 0 );

		if ( pads.Pressed( GESS_PAD_A ) )	PressCurrent();
		if ( pads.Pressed( GESS_PAD_X ) )	Backspace();
		if ( pads.Pressed( GESS_PAD_Y ) )	TypeChar( ' ' );
		if ( pads.Pressed( GESS_PAD_LB ) || pads.Pressed( GESS_PAD_LSTICK ) )	m_bShift = !m_bShift;
		if ( pads.Pressed( GESS_PAD_RB ) )	Finish( true );
		else if ( pads.Pressed( GESS_PAD_B ) )	Finish( false );
	}

	// Physical keyboard still works while the on-screen one is open
	virtual void OnKeyTyped( wchar_t unichar )
	{
		if ( unichar >= 32 && unichar < 127 )
			TypeChar( (char) unichar );
	}

	virtual void OnKeyCodePressed( KeyCode code )
	{
		if ( code == KEY_BACKSPACE )
			Backspace();
		else if ( code == KEY_ENTER || code == KEY_PAD_ENTER )
			Finish( true );
		else if ( code == KEY_ESCAPE )
			Finish( false );
		else
			BaseClass::OnKeyCodePressed( code );
	}

	virtual void OnMousePressed( MouseCode code )
	{
		if ( code != MOUSE_LEFT )
			return;

		int mx, my;
		input()->GetCursorPos( mx, my );
		ScreenToLocal( mx, my );

		int row, col;
		if ( HitTest( mx, my, row, col ) )
		{
			m_iRow = row;
			m_iCol = col;
			PressCurrent();
		}
	}

	virtual void Paint()
	{
		const int w = GetWide(), h = GetTall();

		surface()->DrawSetColor( Color( 20, 20, 20, 245 ) );
		surface()->DrawFilledRect( 0, 0, w, h );
		surface()->DrawSetColor( Color( 185, 168, 90, 255 ) );
		surface()->DrawOutlinedRect( 0, 0, w, h );

		// Title and the text being typed
		DrawString( m_hSmallFont, 12, 8, Color( 230, 200, 80, 255 ), m_szTitle );

		const int fieldY = 30, fieldH = 32;
		surface()->DrawSetColor( Color( 38, 38, 38, 255 ) );
		surface()->DrawFilledRect( 12, fieldY, w - 12, fieldY + fieldH );
		surface()->DrawSetColor( Color( 90, 90, 90, 255 ) );
		surface()->DrawOutlinedRect( 12, fieldY, w - 12, fieldY + fieldH );

		char shown[80];
		const bool bBlink = ( (int)( gpGlobals->realtime * 2.0f ) & 1 ) == 0;
		Q_snprintf( shown, sizeof( shown ), "%s%s", m_szText, bBlink ? "_" : " " );
		DrawString( m_hFont, 20, fieldY + ( fieldH - surface()->GetFontTall( m_hFont ) ) / 2, Color( 255, 255, 255, 255 ), shown );

		char count[16];
		Q_snprintf( count, sizeof( count ), "%d/%d", Q_strlen( m_szText ), m_iMaxLen );
		DrawString( m_hSmallFont, w - 12 - TextWide( m_hSmallFont, count ) - 6, fieldY + fieldH + 2, Color( 150, 150, 150, 255 ), count );

		// Keys
		for ( int row = 0; row < GESS_OSK_ROWS; row++ )
		{
			const int keys = row < 4 ? GESS_OSK_COLS : GESS_OSK_SPECIAL_COUNT;
			for ( int k = 0; k < keys; k++ )
			{
				int x0, y0, x1, y1;
				KeyRect( row, k, x0, y0, x1, y1 );

				const bool bFocus = row == m_iRow && ( row < 4 ? k == m_iCol : k == SpecialAt( m_iCol ) );
				const bool bLit = row == 4 && k == GESS_OSK_SHIFT && m_bShift;
				surface()->DrawSetColor( bFocus ? Color( 106, 100, 64, 255 ) : ( bLit ? Color( 70, 66, 46, 255 ) : Color( 52, 52, 52, 255 ) ) );
				surface()->DrawFilledRect( x0, y0, x1, y1 );
				surface()->DrawSetColor( bFocus ? Color( 230, 200, 80, 255 ) : Color( 90, 90, 90, 255 ) );
				surface()->DrawOutlinedRect( x0, y0, x1, y1 );

				char label[16];
				if ( row < 4 )
					Q_snprintf( label, sizeof( label ), "%c", ( m_bShift ? s_OSKUpper : s_OSKLower )[row][k] );
				else
					Q_strncpy( label, s_OSKSpecialNames[k], sizeof( label ) );

				HFont font = row < 4 ? m_hFont : m_hSmallFont;
				DrawString( font, ( x0 + x1 - TextWide( font, label ) ) / 2, ( y0 + y1 - surface()->GetFontTall( font ) ) / 2, Color( 255, 255, 255, 255 ), label );
			}
		}

		const char *hint = "A: type   X: delete   Y: space   LB: shift   RB: done   B: cancel";
		DrawString( m_hSmallFont, ( w - TextWide( m_hSmallFont, hint ) ) / 2, h - 22, Color( 190, 190, 190, 255 ), hint );
	}

private:
	CGESSMenuPadReader m_Pads;

	int TextWide( HFont font, const char *text )
	{
		wchar_t wbuf[128];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
		int tw, th;
		surface()->GetTextSize( font, wbuf, tw, th );
		return tw;
	}

	void DrawString( HFont font, int x, int y, Color col, const char *text )
	{
		wchar_t wbuf[128];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
		surface()->DrawSetTextFont( font );
		surface()->DrawSetTextColor( col );
		surface()->DrawSetTextPos( x, y );
		surface()->DrawPrintText( wbuf, wcslen( wbuf ) );
	}

	int SpecialAt( int col ) const
	{
		for ( int k = 0; k < GESS_OSK_SPECIAL_COUNT; k++ )
		{
			if ( col >= s_OSKSpecialCol[k] && col < s_OSKSpecialCol[k + 1] )
				return k;
		}
		return GESS_OSK_SPECIAL_COUNT - 1;
	}

	// Key rectangles: rows of 10 equal keys, the bottom row split into the special keys
	void KeyRect( int row, int key, int &x0, int &y0, int &x1, int &y1 )
	{
		const int top = 82, margin = 12, gap = 4;
		const int keyW = ( GetWide() - margin * 2 - gap * ( GESS_OSK_COLS - 1 ) ) / GESS_OSK_COLS;
		const int keyH = ( GetTall() - top - 30 - gap * ( GESS_OSK_ROWS - 1 ) ) / GESS_OSK_ROWS;

		const int c0 = row < 4 ? key : s_OSKSpecialCol[key];
		const int c1 = row < 4 ? key + 1 : s_OSKSpecialCol[key + 1];
		x0 = margin + c0 * ( keyW + gap );
		x1 = margin + c1 * ( keyW + gap ) - gap;
		y0 = top + row * ( keyH + gap );
		y1 = y0 + keyH;
	}

	bool HitTest( int x, int y, int &row, int &col )
	{
		for ( int r = 0; r < GESS_OSK_ROWS; r++ )
		{
			const int keys = r < 4 ? GESS_OSK_COLS : GESS_OSK_SPECIAL_COUNT;
			for ( int k = 0; k < keys; k++ )
			{
				int x0, y0, x1, y1;
				KeyRect( r, k, x0, y0, x1, y1 );
				if ( x >= x0 && x < x1 && y >= y0 && y < y1 )
				{
					row = r;
					col = r < 4 ? k : s_OSKSpecialCol[k];
					return true;
				}
			}
		}
		return false;
	}

	void Move( int dx, int dy )
	{
		if ( dy )
		{
			m_iRow = ( m_iRow + dy + GESS_OSK_ROWS ) % GESS_OSK_ROWS;
		}
		else if ( m_iRow < 4 )
		{
			m_iCol = ( m_iCol + dx + GESS_OSK_COLS ) % GESS_OSK_COLS;
		}
		else
		{
			// Bottom row moves key by key
			const int k = ( SpecialAt( m_iCol ) + dx + GESS_OSK_SPECIAL_COUNT ) % GESS_OSK_SPECIAL_COUNT;
			m_iCol = s_OSKSpecialCol[k];
		}
	}

	void PressCurrent()
	{
		if ( m_iRow < 4 )
		{
			TypeChar( ( m_bShift ? s_OSKUpper : s_OSKLower )[m_iRow][m_iCol] );
			return;
		}

		switch ( SpecialAt( m_iCol ) )
		{
		case GESS_OSK_SHIFT:	m_bShift = !m_bShift; break;
		case GESS_OSK_SPACE:	TypeChar( ' ' ); break;
		case GESS_OSK_DELETE:	Backspace(); break;
		default:				Finish( true ); break;
		}
	}

	void TypeChar( char c )
	{
		const int len = Q_strlen( m_szText );
		if ( len >= m_iMaxLen )
			return;
		m_szText[len] = c;
		m_szText[len + 1] = 0;

		// Capitalise the first letter only, like a name
		if ( len == 0 && m_bShift && c != ' ' )
			m_bShift = false;
	}

	void Backspace()
	{
		const int len = Q_strlen( m_szText );
		if ( len > 0 )
			m_szText[len - 1] = 0;
	}

	void Finish( bool bAccept )
	{
		if ( m_pTarget )
			m_pTarget->SetText( bAccept ? m_szText : m_szOriginal );

		SetVisible( false );
		PostActionSignal( new KeyValues( "KeyboardDone", "accepted", bAccept ? 1 : 0 ) );
	}

	TextEntry	*m_pTarget;
	char		m_szText[64];
	char		m_szOriginal[64];
	char		m_szTitle[64];
	int			m_iMaxLen;
	int			m_iRow, m_iCol;
	bool		m_bShift;
	HFont		m_hFont, m_hSmallFont;
};

//-----------------------------------------------------------------------------
// Character picker: GoldenEye's character select look (the portrait grid and a
// big preview with name and bio), but it only picks the character for a profile.
// Controller: d-pad / stick to move, A to choose, B to cancel. Mouse: click to
// look at a character, double click (or Enter) to choose.
//-----------------------------------------------------------------------------
class CGESSCharacterPicker : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGESSCharacterPicker, vgui::Panel );

public:
	CGESSCharacterPicker( Panel *pParent ) : BaseClass( pParent, "GESSCharacterPicker" )
	{
		m_iSel = 0;
		m_iCols = 6;
		m_iFirstRow = 0;
		m_hFont = m_hSmallFont = INVALID_FONT;
		SetVisible( false );
		SetMouseInputEnabled( true );
		SetKeyBoardInputEnabled( true );
		SetPaintBackgroundEnabled( false );
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		m_hFont = pScheme->GetFont( "DefaultLarge", false );
		if ( m_hFont == INVALID_FONT )
			m_hFont = pScheme->GetFont( "Default", false );
		m_hSmallFont = pScheme->GetFont( "Default", false );
	}

	void Open( const char *currentIdent )
	{
		// Random first, then everyone (both teams: in team games the server picks a
		// fitting character when the profile's one belongs to the other team)
		m_Chars.RemoveAll();
		const CGECharData *pRandom = GECharacters()->GetRandomChar();
		if ( pRandom )
			m_Chars.AddToTail( pRandom );
		for ( int i = 0; i < GECharacters()->Count(); i++ )
		{
			const CGECharData *pChar = GECharacters()->Get( i );
			if ( pChar && pChar != pRandom && Q_stricmp( pChar->szIdentifier, CHAR_RANDOM_IDENT ) )
				m_Chars.AddToTail( pChar );
		}

		m_iSel = 0;
		for ( int i = 0; i < m_Chars.Count(); i++ )
		{
			if ( currentIdent && !Q_stricmp( m_Chars[i]->szIdentifier, currentIdent ) )
				m_iSel = i;
		}

		m_iFirstRow = 0;
		SetVisible( true );
		MoveToFront();
		RequestFocus();
	}

	// vgui::Panel::IsVisible() is not const in the SDK
	bool IsOpen() const { return const_cast<CGESSCharacterPicker *>( this )->IsVisible(); }

	void HandlePads( const CGESSMenuPadReader &pads )
	{
		if ( pads.Dir( GESS_DIR_UP ) )		Move( 0, -1 );
		if ( pads.Dir( GESS_DIR_DOWN ) )	Move( 0, 1 );
		if ( pads.Dir( GESS_DIR_LEFT ) )	Move( -1, 0 );
		if ( pads.Dir( GESS_DIR_RIGHT ) )	Move( 1, 0 );

		if ( pads.Pressed( GESS_PAD_A ) || pads.Pressed( GESS_PAD_START ) )
			Finish( true );
		else if ( pads.Pressed( GESS_PAD_B ) )
			Finish( false );
	}

	virtual void OnKeyCodePressed( KeyCode code )
	{
		if ( code == KEY_ENTER || code == KEY_PAD_ENTER )		Finish( true );
		else if ( code == KEY_ESCAPE )							Finish( false );
		else if ( code == KEY_UP )								Move( 0, -1 );
		else if ( code == KEY_DOWN )							Move( 0, 1 );
		else if ( code == KEY_LEFT )							Move( -1, 0 );
		else if ( code == KEY_RIGHT )							Move( 1, 0 );
		else BaseClass::OnKeyCodePressed( code );
	}

	virtual void OnMousePressed( MouseCode code )
	{
		if ( code != MOUSE_LEFT )
			return;
		const int i = HitCell();
		if ( i >= 0 )
			m_iSel = i;
		else if ( HitButton( true ) )
			Finish( true );
		else if ( HitButton( false ) )
			Finish( false );
	}

	virtual void OnMouseDoublePressed( MouseCode code )
	{
		const int i = HitCell();
		if ( code == MOUSE_LEFT && i >= 0 )
		{
			m_iSel = i;
			Finish( true );
		}
	}

	virtual void OnMouseWheeled( int delta )
	{
		Move( 0, delta > 0 ? -1 : 1 );
	}

	virtual void Paint()
	{
		const int w = GetWide(), h = GetTall();

		surface()->DrawSetColor( Color( 20, 20, 20, 255 ) );
		surface()->DrawFilledRect( 0, 0, w, h );
		surface()->DrawSetColor( Color( 185, 168, 90, 255 ) );
		surface()->DrawOutlinedRect( 0, 0, w, h );
		DrawString( m_hSmallFont, 12, 8, Color( 230, 200, 80, 255 ), "Choose a character" );

		// Portrait grid
		int cell, gx, gy, rows;
		GridLayout( cell, gx, gy, rows );
		EnsureVisible( rows );
		for ( int r = 0; r < rows; r++ )
		{
			for ( int c = 0; c < m_iCols; c++ )
			{
				const int i = ( m_iFirstRow + r ) * m_iCols + c;
				if ( i >= m_Chars.Count() )
					break;
				const int x = gx + c * cell, y = gy + r * cell;
				const bool bSel = i == m_iSel;
				DrawPortrait( m_Chars[i], bSel, x + 2, y + 2, cell - 4, cell - 4 );
				surface()->DrawSetColor( bSel ? Color( 230, 200, 80, 255 ) : Color( 70, 70, 70, 255 ) );
				surface()->DrawOutlinedRect( x + 1, y + 1, x + cell - 1, y + cell - 1 );
				if ( bSel )
					surface()->DrawOutlinedRect( x, y, x + cell, y + cell );
			}
		}

		// More rows above / below
		const int totalRows = ( m_Chars.Count() + m_iCols - 1 ) / m_iCols;
		if ( m_iFirstRow > 0 )
			DrawString( m_hSmallFont, gx + m_iCols * cell / 2 - 4, gy - 16, Color( 230, 200, 80, 255 ), "^" );
		if ( m_iFirstRow + rows < totalRows )
			DrawString( m_hSmallFont, gx + m_iCols * cell / 2 - 4, gy + rows * cell + 2, Color( 230, 200, 80, 255 ), "v" );

		// Preview of the selected character
		if ( m_Chars.IsValidIndex( m_iSel ) )
		{
			const CGECharData *pChar = m_Chars[m_iSel];
			const int px = gx + m_iCols * cell + 20;
			const int pw = w - px - 14;
			const int pSize = Min( pw, h / 2 );
			const int pLeft = px + ( pw - pSize ) / 2;
			DrawPortrait( pChar, true, pLeft, 34, pSize, pSize );
			surface()->DrawSetColor( Color( 185, 168, 90, 255 ) );
			surface()->DrawOutlinedRect( pLeft - 1, 33, pLeft + pSize + 1, 35 + pSize );

			char name[128];
			Localized( pChar == GECharacters()->GetRandomChar() ? "Random" : pChar->szPrintName, name, sizeof( name ) );
			int y = 44 + pSize;
			DrawString( m_hFont, px + ( pw - TextWide( m_hFont, name ) ) / 2, y, Color( 255, 255, 255, 255 ), name );
			y += surface()->GetFontTall( m_hFont ) + 6;

			char bio[1024];
			Localized( pChar->szBio, bio, sizeof( bio ) );
			DrawWrapped( m_hSmallFont, px, y, pw, h - 60 - y, Color( 200, 200, 200, 255 ), bio );
		}

		// Choose / Cancel
		for ( int b = 0; b < 2; b++ )
		{
			int x0, y0, x1, y1;
			ButtonRect( b == 0, x0, y0, x1, y1 );
			surface()->DrawSetColor( Color( 52, 52, 52, 255 ) );
			surface()->DrawFilledRect( x0, y0, x1, y1 );
			surface()->DrawSetColor( Color( 120, 110, 70, 255 ) );
			surface()->DrawOutlinedRect( x0, y0, x1, y1 );
			const char *label = b == 0 ? "Choose" : "Cancel";
			DrawString( m_hSmallFont, ( x0 + x1 - TextWide( m_hSmallFont, label ) ) / 2, ( y0 + y1 - surface()->GetFontTall( m_hSmallFont ) ) / 2, Color( 255, 255, 255, 255 ), label );
		}

		DrawString( m_hSmallFont, 12, h - 22, Color( 190, 190, 190, 255 ), "D-pad / stick: move   A: choose   B: cancel" );
	}

private:
	void Move( int dx, int dy )
	{
		const int count = m_Chars.Count();
		if ( count <= 0 )
			return;
		int i = m_iSel + dx + dy * m_iCols;
		if ( dx && ( i < 0 || i >= count ) )
			i = ( i + count ) % count;
		if ( dy && ( i < 0 || i >= count ) )
			return;
		m_iSel = i;
	}

	void Finish( bool bAccepted )
	{
		SetVisible( false );
		KeyValues *kv = new KeyValues( "CharacterPicked" );
		kv->SetInt( "accepted", bAccepted ? 1 : 0 );
		kv->SetString( "ident", ( bAccepted && m_Chars.IsValidIndex( m_iSel ) ) ? m_Chars[m_iSel]->szIdentifier : "" );
		PostActionSignal( kv );
	}

	// Grid on the left ~60% of the panel, square cells
	void GridLayout( int &cell, int &gx, int &gy, int &rows )
	{
		const int w = GetWide(), h = GetTall();
		gx = 14;
		gy = 34;
		const int gridW = w * 6 / 10 - gx;
		const int gridH = h - gy - 60;
		cell = Max( 24, gridW / m_iCols );
		rows = Max( 1, gridH / cell );
	}

	void EnsureVisible( int rows )
	{
		const int row = m_iSel / m_iCols;
		if ( row < m_iFirstRow )
			m_iFirstRow = row;
		else if ( row >= m_iFirstRow + rows )
			m_iFirstRow = row - rows + 1;
	}

	int HitCell()
	{
		int mx, my;
		input()->GetCursorPos( mx, my );
		ScreenToLocal( mx, my );
		int cell, gx, gy, rows;
		GridLayout( cell, gx, gy, rows );
		if ( mx < gx || my < gy || mx >= gx + m_iCols * cell || my >= gy + rows * cell )
			return -1;
		const int i = ( m_iFirstRow + ( my - gy ) / cell ) * m_iCols + ( mx - gx ) / cell;
		return i < m_Chars.Count() ? i : -1;
	}

	void ButtonRect( bool bChoose, int &x0, int &y0, int &x1, int &y1 )
	{
		const int w = GetWide(), h = GetTall();
		x1 = bChoose ? w - 14 - 110 : w - 14;
		x0 = x1 - 100;
		y0 = h - 44;
		y1 = h - 14;
	}

	bool HitButton( bool bChoose )
	{
		int mx, my;
		input()->GetCursorPos( mx, my );
		ScreenToLocal( mx, my );
		int x0, y0, x1, y1;
		ButtonRect( bChoose, x0, y0, x1, y1 );
		return mx >= x0 && mx < x1 && my >= y0 && my < y1;
	}

	// The same portraits as GoldenEye's character select
	void DrawPortrait( const CGECharData *pChar, bool bSelected, int x, int y, int w, int h )
	{
		char image[128];
		Q_snprintf( image, sizeof( image ), "charselect/%s_portrait_%s", pChar->szIdentifier, bSelected ? "S" : "N" );
		IImage *pImage = scheme()->GetImage( image, false );
		if ( !pImage )
			return;
		pImage->SetPos( x, y );
		pImage->SetSize( w, h );
		pImage->Paint();
	}

	static void Localized( const char *text, char *out, int len )
	{
		const wchar_t *pw = ( text && text[0] == '#' ) ? g_pVGuiLocalize->Find( text ) : NULL;
		if ( pw )
			g_pVGuiLocalize->ConvertUnicodeToANSI( pw, out, len );
		else
			Q_strncpy( out, text ? text : "", len );
	}

	int TextWide( HFont font, const char *text )
	{
		wchar_t wbuf[512];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
		int tw, th;
		surface()->GetTextSize( font, wbuf, tw, th );
		return tw;
	}

	void DrawString( HFont font, int x, int y, Color col, const char *text )
	{
		wchar_t wbuf[512];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
		surface()->DrawSetTextFont( font );
		surface()->DrawSetTextColor( col );
		surface()->DrawSetTextPos( x, y );
		surface()->DrawPrintText( wbuf, wcslen( wbuf ) );
	}

	// Word wrapped text inside a box
	void DrawWrapped( HFont font, int x, int y, int w, int h, Color col, const char *text )
	{
		const int lineTall = surface()->GetFontTall( font ) + 2;
		char line[256] = "";
		const char *p = text;
		while ( *p && h >= lineTall )
		{
			// Next word (a newline ends the line)
			while ( *p == ' ' ) p++;
			const char *wordEnd = p;
			while ( *wordEnd && *wordEnd != ' ' && *wordEnd != '\n' ) wordEnd++;

			char test[256];
			Q_snprintf( test, sizeof( test ), "%s%s%.*s", line, line[0] ? " " : "", (int)( wordEnd - p ), p );
			if ( line[0] && TextWide( font, test ) > w )
			{
				DrawString( font, x, y, col, line );
				y += lineTall; h -= lineTall;
				line[0] = 0;
				continue;	// same word on the next line
			}
			Q_strncpy( line, test, sizeof( line ) );
			p = wordEnd;
			if ( *p == '\n' )
			{
				DrawString( font, x, y, col, line );
				y += lineTall; h -= lineTall;
				line[0] = 0;
				p++;
			}
		}
		if ( line[0] && h >= lineTall )
			DrawString( font, x, y, col, line );
	}

	CUtlVector<const CGECharData *> m_Chars;
	int m_iSel;
	int m_iCols;
	int m_iFirstRow;
	HFont m_hFont, m_hSmallFont;
};

//-----------------------------------------------------------------------------
// A text field that opens the on-screen keyboard when clicked while a
// controller is connected (controller 1's trigger clicks like a mouse)
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Shown the first time the Split Screen menu opens (until "Don't show this again")
//-----------------------------------------------------------------------------
ConVar ges_ss_hide_intro( "ges_ss_hide_intro", "0", FCVAR_ARCHIVE, "1: don't show the split screen introduction when the Split Screen menu opens." );

class CGESSIntroPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGESSIntroPanel, vgui::Panel );

public:
	CGESSIntroPanel( Panel *pParent ) : BaseClass( pParent, "GESSIntro" )
	{
		m_hFont = m_hSmallFont = INVALID_FONT;
		m_iFocus = 1;
		m_bDontShow = false;
		SetVisible( false );
		SetMouseInputEnabled( true );
		SetKeyBoardInputEnabled( true );
		SetPaintBackgroundEnabled( false );
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		m_hFont = pScheme->GetFont( "DefaultLarge", false );
		if ( m_hFont == INVALID_FONT )
			m_hFont = pScheme->GetFont( "Default", false );
		m_hSmallFont = pScheme->GetFont( "Default", false );
	}

	void Open()
	{
		m_iFocus = 1;	// OK
		m_bDontShow = false;
		SetVisible( true );
		MoveToFront();
		RequestFocus();
	}

	bool IsOpen() const { return const_cast<CGESSIntroPanel *>( this )->IsVisible(); }

	void HandlePads( const CGESSMenuPadReader &pads )
	{
		if ( pads.Dir( GESS_DIR_UP ) || pads.Dir( GESS_DIR_DOWN ) || pads.Dir( GESS_DIR_LEFT ) || pads.Dir( GESS_DIR_RIGHT ) )
			m_iFocus = 1 - m_iFocus;

		if ( pads.Pressed( GESS_PAD_A ) )
		{
			if ( m_iFocus == 0 )
				m_bDontShow = !m_bDontShow;
			else
				Finish();
		}
		else if ( pads.Pressed( GESS_PAD_B ) || pads.Pressed( GESS_PAD_START ) )
		{
			Finish();
		}
	}

	virtual void OnKeyCodePressed( KeyCode code )
	{
		if ( code == KEY_ENTER || code == KEY_PAD_ENTER || code == KEY_ESCAPE )
			Finish();
		else if ( code == KEY_SPACE )
			m_bDontShow = !m_bDontShow;
		else if ( code == KEY_UP || code == KEY_DOWN || code == KEY_TAB )
			m_iFocus = 1 - m_iFocus;
		else
			BaseClass::OnKeyCodePressed( code );
	}

	virtual void OnMousePressed( MouseCode code )
	{
		if ( code != MOUSE_LEFT )
			return;
		int x0, y0, x1, y1;
		if ( Hit( 0, x0, y0, x1, y1 ) )
		{
			m_iFocus = 0;
			m_bDontShow = !m_bDontShow;
		}
		else if ( Hit( 1, x0, y0, x1, y1 ) )
		{
			Finish();
		}
	}

	virtual void Paint()
	{
		const int w = GetWide(), h = GetTall();

		// Dim the menu behind, then the message box
		surface()->DrawSetColor( Color( 0, 0, 0, 200 ) );
		surface()->DrawFilledRect( 0, 0, w, h );

		int bx0, by0, bx1, by1;
		BoxRect( bx0, by0, bx1, by1 );
		surface()->DrawSetColor( Color( 20, 20, 20, 255 ) );
		surface()->DrawFilledRect( bx0, by0, bx1, by1 );
		surface()->DrawSetColor( Color( 185, 168, 90, 255 ) );
		surface()->DrawOutlinedRect( bx0, by0, bx1, by1 );

		const int pad = 18;
		int y = by0 + pad;
		DrawString( m_hFont, bx0 + pad, y, Color( 230, 200, 80, 255 ), "Split screen is played with controllers" );
		y += surface()->GetFontTall( m_hFont ) + 12;

		DrawWrapped( m_hSmallFont, bx0 + pad, y, bx1 - bx0 - pad * 2, by1 - y - 90, Color( 230, 230, 230, 255 ),
			"Keyboard and mouse are turned off during split screen matches (Esc and the console still work).\n"
			"\n"
			"Player 1 is the first controller to press a button. The other players press START here to join.\n"
			"\n"
			"Everyone chooses their profile here with LB / RB. Y switches between the Profiles and Match pages, where player 1 sets how many players share each display.\n"
			"\n"
			"When the match loads, each player takes their screen with the right trigger (and picks a team with the left stick). The match starts once everyone is in." );

		// "Don't show this again" tick box and OK
		int x0, y0, x1, y1;
		Rect( 0, x0, y0, x1, y1 );
		const int box = y1 - y0;
		surface()->DrawSetColor( Color( 52, 52, 52, 255 ) );
		surface()->DrawFilledRect( x0, y0, x0 + box, y1 );
		surface()->DrawSetColor( Color( 160, 150, 100, 255 ) );
		surface()->DrawOutlinedRect( x0, y0, x0 + box, y1 );
		if ( m_bDontShow )
			DrawString( m_hSmallFont, x0 + ( box - TextWide( m_hSmallFont, "X" ) ) / 2, y0 + ( box - surface()->GetFontTall( m_hSmallFont ) ) / 2, Color( 230, 200, 80, 255 ), "X" );
		DrawString( m_hSmallFont, x0 + box + 8, y0 + ( box - surface()->GetFontTall( m_hSmallFont ) ) / 2, Color( 255, 255, 255, 255 ), "Don't show this again" );

		Rect( 1, x0, y0, x1, y1 );
		surface()->DrawSetColor( Color( 52, 52, 52, 255 ) );
		surface()->DrawFilledRect( x0, y0, x1, y1 );
		surface()->DrawSetColor( Color( 120, 110, 70, 255 ) );
		surface()->DrawOutlinedRect( x0, y0, x1, y1 );
		DrawString( m_hSmallFont, ( x0 + x1 - TextWide( m_hSmallFont, "OK" ) ) / 2, ( y0 + y1 - surface()->GetFontTall( m_hSmallFont ) ) / 2, Color( 255, 255, 255, 255 ), "OK" );

		// Gold outline on the controller's choice
		Rect( m_iFocus, x0, y0, x1, y1 );
		surface()->DrawSetColor( Color( 230, 200, 80, 255 ) );
		surface()->DrawOutlinedRect( x0 - 3, y0 - 3, x1 + 3, y1 + 3 );
		surface()->DrawOutlinedRect( x0 - 2, y0 - 2, x1 + 2, y1 + 2 );

		DrawString( m_hSmallFont, bx0 + pad, by1 - 22, Color( 190, 190, 190, 255 ), "D-pad: move   A: select   B: close" );
	}

private:
	void Finish()
	{
		if ( m_bDontShow )
		{
			ges_ss_hide_intro.SetValue( 1 );
			engine->ClientCmd_Unrestricted( "host_writeconfig\n" );
		}
		SetVisible( false );
		if ( GetParent() )
			GetParent()->RequestFocus();
	}

	void BoxRect( int &x0, int &y0, int &x1, int &y1 )
	{
		const int w = GetWide(), h = GetTall();
		const int bw = Min( w - 40, 560 ), bh = Min( h - 40, 440 );
		x0 = ( w - bw ) / 2; y0 = ( h - bh ) / 2;
		x1 = x0 + bw; y1 = y0 + bh;
	}

	// 0: tick box row, 1: OK button
	void Rect( int which, int &x0, int &y0, int &x1, int &y1 )
	{
		int bx0, by0, bx1, by1;
		BoxRect( bx0, by0, bx1, by1 );
		const int rowTall = Max( 22, surface()->GetFontTall( m_hSmallFont ) + 8 );
		y1 = by1 - 34;
		y0 = y1 - rowTall;
		if ( which == 0 )
		{
			x0 = bx0 + 18;
			x1 = x0 + rowTall + 8 + TextWide( m_hSmallFont, "Don't show this again" );
		}
		else
		{
			x1 = bx1 - 18;
			x0 = x1 - 100;
		}
	}

	bool Hit( int which, int &x0, int &y0, int &x1, int &y1 )
	{
		int mx, my;
		input()->GetCursorPos( mx, my );
		ScreenToLocal( mx, my );
		Rect( which, x0, y0, x1, y1 );
		return mx >= x0 && mx < x1 && my >= y0 && my < y1;
	}

	int TextWide( HFont font, const char *text )
	{
		wchar_t wbuf[512];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
		int tw, th;
		surface()->GetTextSize( font, wbuf, tw, th );
		return tw;
	}

	void DrawString( HFont font, int x, int y, Color col, const char *text )
	{
		wchar_t wbuf[512];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
		surface()->DrawSetTextFont( font );
		surface()->DrawSetTextColor( col );
		surface()->DrawSetTextPos( x, y );
		surface()->DrawPrintText( wbuf, wcslen( wbuf ) );
	}

	void DrawWrapped( HFont font, int x, int y, int w, int h, Color col, const char *text )
	{
		const int lineTall = surface()->GetFontTall( font ) + 2;
		char line[256] = "";
		const char *p = text;
		while ( *p && h >= lineTall )
		{
			while ( *p == ' ' ) p++;
			if ( *p == '\n' )
			{
				if ( line[0] )
					DrawString( font, x, y, col, line );
				y += lineTall; h -= lineTall;
				line[0] = 0;
				p++;
				continue;
			}
			const char *wordEnd = p;
			while ( *wordEnd && *wordEnd != ' ' && *wordEnd != '\n' ) wordEnd++;

			char test[256];
			Q_snprintf( test, sizeof( test ), "%s%s%.*s", line, line[0] ? " " : "", (int)( wordEnd - p ), p );
			if ( line[0] && TextWide( font, test ) > w )
			{
				DrawString( font, x, y, col, line );
				y += lineTall; h -= lineTall;
				line[0] = 0;
				continue;
			}
			Q_strncpy( line, test, sizeof( line ) );
			p = wordEnd;
		}
		if ( line[0] && h >= lineTall )
			DrawString( font, x, y, col, line );
	}

	vgui::HFont m_hFont, m_hSmallFont;
	int m_iFocus;		// 0: tick box, 1: OK
	bool m_bDontShow;
};

//-----------------------------------------------------------------------------
// "Players per display" goes up to the number of players who have joined in the
// Split Screen menu (player 1 and everyone who pressed START), shared across
// the displays, display 1 first
//-----------------------------------------------------------------------------
static ConVar ges_ss_debug_joined( "ges_ss_debug_joined", "-1", FCVAR_DONTRECORD, "Testing: pretend this many players have joined, for the Split Screen menu's players per display (-1: the real count)." );

static int GESS_MenuPadCount()
{
	if ( ges_ss_debug_joined.GetInt() >= 0 )
		return ges_ss_debug_joined.GetInt();

	CGESplitScreenClient *pSS = GESplitScreenClient();
	int joined = 1;		// player 1
	for ( int slot = 1; slot < GES_SS_MAX_PLAYERS; slot++ )
	{
		if ( pSS->GetSlotState( slot ) != GESS_SLOT_EMPTY )
			joined++;
	}
	return joined;
}

// The largest "players per display" the joined players can fill (at least 1)
static int GESS_PerDisplayLimit()
{
	const int displays = Max( 1, GESplitScreenClient()->GetDisplayCount() );
	const int pads = clamp( GESS_MenuPadCount(), 1, GES_SS_MAX_PLAYERS );
	return clamp( ( pads + displays - 1 ) / displays, 1, GES_SS_MAX_PLAYERS );
}

//-----------------------------------------------------------------------------
// Diagram of the displays and how they are split ("players per display")
//-----------------------------------------------------------------------------
class CGESSLayoutDiagram : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGESSLayoutDiagram, vgui::Panel );

public:
	CGESSLayoutDiagram( Panel *pParent ) : BaseClass( pParent, "GESSLayoutDiagram" )
	{
		m_hFont = m_hSmallFont = INVALID_FONT;
		SetPaintBackgroundEnabled( false );
		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		m_hFont = pScheme->GetFont( "DefaultLarge", false );
		if ( m_hFont == INVALID_FONT )
			m_hFont = pScheme->GetFont( "Default", false );
		m_hSmallFont = pScheme->GetFont( "Default", false );
	}

	virtual void Paint()
	{
		CGESplitScreenClient *pSS = GESplitScreenClient();
		const int displays = pSS->GetDisplayCount();
		const int perDisplay = clamp( GESSMatch()->Settings().perdisplay, 1, GES_SS_MAX_PLAYERS );
		// Numbered boxes only for the players who joined (with 2 displays
		// and an odd count the last box is spare and shows the scores)
		const int total = MIN( MIN( GES_SS_MAX_PLAYERS, perDisplay * displays ), clamp( GESS_MenuPadCount(), 1, GES_SS_MAX_PLAYERS ) );

		const int w = GetWide(), h = GetTall();
		const int textTall = surface()->GetFontTall( m_hSmallFont );
		const int gap = 12;

		// Displays as 16:9 screens side by side, as big as fit
		int dw = ( w - gap * ( displays - 1 ) ) / displays;
		int dh = dw * 9 / 16;
		if ( dh > h - textTall - 8 )
		{
			dh = h - textTall - 8;
			dw = dh * 16 / 9;
		}
		const int totalW = dw * displays + gap * ( displays - 1 );
		const int x0 = ( w - totalW ) / 2;

		int cols, rows;
		CGESplitScreenClient::GridForCount( perDisplay, cols, rows );
		bool bScoresShown = false;

		for ( int d = 0; d < displays; d++ )
		{
			const int dx = x0 + d * ( dw + gap );
			surface()->DrawSetColor( Color( 15, 15, 15, 255 ) );
			surface()->DrawFilledRect( dx, 0, dx + dw, dh );

			for ( int r = 0; r < rows; r++ )
			{
				for ( int c = 0; c < cols; c++ )
				{
					const int cell = r * cols + c;
					const int slot = d * perDisplay + cell;
					const int cx0 = dx + 3 + ( c * ( dw - 6 ) ) / cols, cx1 = dx + 3 + ( ( c + 1 ) * ( dw - 6 ) ) / cols;
					const int cy0 = 3 + ( r * ( dh - 6 ) ) / rows, cy1 = 3 + ( ( r + 1 ) * ( dh - 6 ) ) / rows;
					const bool bPlayer = cell < perDisplay && slot < total;

					surface()->DrawSetColor( bPlayer ? ( slot == 0 ? Color( 150, 125, 50, 255 ) : Color( 70, 85, 110, 255 ) ) : Color( 45, 45, 45, 255 ) );
					surface()->DrawFilledRect( cx0 + 1, cy0 + 1, cx1 - 1, cy1 - 1 );
					surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
					surface()->DrawOutlinedRect( cx0, cy0, cx1, cy1 );

					// The first cell nobody plays in shows the scores, the rest the logo
					const char *label = bPlayer ? VarArgs( "%d", slot + 1 ) : ( bScoresShown ? "" : "Scores" );
					if ( !bPlayer )
						bScoresShown = true;
					HFont font = bPlayer ? m_hFont : m_hSmallFont;
					if ( !bPlayer && TextWide( font, label ) > cx1 - cx0 - 4 )
						label = "-";
					DrawString( font, ( cx0 + cx1 - TextWide( font, label ) ) / 2, ( cy0 + cy1 - surface()->GetFontTall( font ) ) / 2,
						Color( 255, 255, 255, bPlayer ? 255 : 150 ), label );
				}
			}

			// Bezel
			surface()->DrawSetColor( Color( 185, 168, 90, 255 ) );
			surface()->DrawOutlinedRect( dx, 0, dx + dw, dh );
		}

		char text[128];
		if ( displays == 2 )
			Q_snprintf( text, sizeof( text ), "2 displays: up to %d players", total );
		else
			Q_snprintf( text, sizeof( text ), "1 display: up to %d players", total );
		DrawString( m_hSmallFont, ( w - TextWide( m_hSmallFont, text ) ) / 2, dh + 6, Color( 230, 230, 230, 255 ), text );
	}

private:
	int TextWide( HFont font, const char *text )
	{
		wchar_t wbuf[128];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
		int tw, th;
		surface()->GetTextSize( font, wbuf, tw, th );
		return tw;
	}

	void DrawString( HFont font, int x, int y, Color col, const char *text )
	{
		wchar_t wbuf[128];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
		surface()->DrawSetTextFont( font );
		surface()->DrawSetTextColor( col );
		surface()->DrawSetTextPos( x, y );
		surface()->DrawPrintText( wbuf, wcslen( wbuf ) );
	}

	vgui::HFont m_hFont, m_hSmallFont;
};

class CGESSKeyboardTextEntry : public TextEntry
{
	DECLARE_CLASS_SIMPLE( CGESSKeyboardTextEntry, TextEntry );

public:
	CGESSKeyboardTextEntry( Panel *pParent, const char *name ) : BaseClass( pParent, name ) {}

	virtual void OnMousePressed( MouseCode code )
	{
		BaseClass::OnMousePressed( code );
		if ( code == MOUSE_LEFT && IsEnabled() && GESSGamepads()->GetConnectedCount() > 0 )
			PostActionSignal( new KeyValues( "OpenKeyboard" ) );
	}
};

//-----------------------------------------------------------------------------
// The local players, in the top right corner of the screen while the Split
// Screen menu is open (on every page): a small gamepad with the player number,
// their profile name and character, and what they can do. LB / RB (change
// profile) and START (join / leave) are read in ge_splitscreen_client.cpp.
//-----------------------------------------------------------------------------
class CGESSPlayerList : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGESSPlayerList, vgui::Panel );

public:
	CGESSPlayerList( vgui::VPANEL parent ) : BaseClass( NULL, "GESSPlayerList" )
	{
		SetParent( parent );
		SetScheme( vgui::scheme()->LoadSchemeFromFile( "resource/SourceScheme.res", "SourceScheme" ) );
		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );
		SetPaintBackgroundEnabled( false );
		SetVisible( false );
		m_hNameFont = m_hSmallFont = m_hNumberFont = INVALID_FONT;
		m_iFontScreenTall = 0;
		m_iRows = 0;
		vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );
	}

	void SetMenu( vgui::Panel *pMenu ) { m_hMenu = pMenu; }

	// Shown exactly while the menu is (it can also be hidden by the game UI
	// closing or a match starting, without OnClose)
	virtual void OnTick()
	{
		const bool bShow = m_hMenu.Get() && m_hMenu->IsVisible() && enginevgui->IsGameUIVisible();
		if ( bShow != IsVisible() )
		{
			SetVisible( bShow );
			if ( bShow && !vgui::input()->GetAppModalSurface() )
				MoveToFront();
		}
		if ( bShow )
			Update();
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		// The scheme's fonts until UpdateFonts makes screen-sized ones
		m_hNameFont = pScheme->GetFont( "DefaultBold", false );
		m_hSmallFont = pScheme->GetFont( "Default", false );
		m_hNumberFont = m_hNameFont;
		m_iFontScreenTall = 0;
	}

	// The scheme's face (Tahoma) sized from the screen height: the scheme's own
	// sizes stop growing at 1080p, and this has to read from the sofa
	void UpdateFonts()
	{
		int sw, sh;
		surface()->GetScreenSize( sw, sh );
		if ( sh == m_iFontScreenTall )
			return;
		m_iFontScreenTall = sh;
		const int flags = vgui::ISurface::FONTFLAG_ANTIALIAS;
		HFont f = surface()->CreateFont();
		if ( surface()->SetFontGlyphSet( f, "Tahoma", Max( 13, sh / 48 ), 700, 0, 0, flags ) )
			m_hNameFont = f;
		f = surface()->CreateFont();
		if ( surface()->SetFontGlyphSet( f, "Tahoma", Max( 11, sh / 64 ), 400, 0, 0, flags ) )
			m_hSmallFont = f;
		f = surface()->CreateFont();
		if ( surface()->SetFontGlyphSet( f, "Tahoma", Max( 12, sh / 46 ), 900, 0, 0, flags ) )
			m_hNumberFont = f;
	}

	// Called by the menu every frame while it is open
	void Update()
	{
		UpdateFonts();
		BuildRows();

		int sw, sh;
		surface()->GetScreenSize( sw, sh );
		const int nameTall = FontTall( m_hNameFont ), smallTall = FontTall( m_hSmallFont );
		const int pad = Max( 6, sh / 120 );
		const int rowTall = Max( nameTall + smallTall + 6, sh / 20 );
		const int iconW = IconWide( rowTall );

		// As wide as the longest line (and the hint), at least a sixth of the screen
		int textW = Max( TextWide( m_hSmallFont, HintText() ), TextWide( m_hNameFont, "Players" ) ) - iconW - pad;
		for ( int i = 0; i < m_iRows; i++ )
		{
			textW = Max( textW, TextWide( m_hNameFont, m_Rows[i].name ) );
			textW = Max( textW, TextWide( m_hSmallFont, m_Rows[i].state ) );
		}
		const int w = Max( sw / 6, pad * 3 + iconW + textW );
		const int h = pad + nameTall + pad / 2 + m_iRows * ( rowTall + pad / 2 ) + pad / 2 + smallTall + pad;
		const int margin = Max( 8, sh / 54 );
		SetBounds( sw - w - margin, margin, w, h );
		Repaint();
	}

	virtual void Paint()
	{
		int sw, sh;
		surface()->GetScreenSize( sw, sh );
		const int w = GetWide(), h = GetTall();
		const int nameTall = FontTall( m_hNameFont ), smallTall = FontTall( m_hSmallFont );
		const int pad = Max( 6, sh / 120 );
		const int rowTall = Max( nameTall + smallTall + 6, sh / 20 );
		const int iconW = IconWide( rowTall );

		// Dark box with the menu's gold edge
		surface()->DrawSetColor( Color( 10, 10, 10, 215 ) );
		surface()->DrawFilledRect( 0, 0, w, h );
		surface()->DrawSetColor( Color( 185, 168, 90, 255 ) );
		surface()->DrawOutlinedRect( 0, 0, w, h );

		int y = pad;
		DrawString( m_hNameFont, pad, y, Color( 230, 200, 80, 255 ), "Players" );
		y += nameTall + pad / 2;

		for ( int i = 0; i < m_iRows; i++ )
		{
			const Row_t &row = m_Rows[i];

			// Row background: player 1 gold like the screen diagram, joined players blue
			surface()->DrawSetColor( row.slot == 0 ? Color( 150, 125, 50, 90 ) : ( row.bIn ? Color( 70, 85, 110, 110 ) : Color( 45, 45, 45, 110 ) ) );
			surface()->DrawFilledRect( pad / 2, y, w - pad / 2, y + rowTall );

			DrawGamepad( pad, y + 2, iconW, rowTall - 4, row.slot + 1, row.bIn, row.slot == 0 );

			const int tx = pad * 2 + iconW;
			const int ty = y + ( rowTall - nameTall - smallTall - 2 ) / 2;
			DrawString( m_hNameFont, tx, ty, Color( 255, 255, 255, row.bIn ? 255 : 150 ), row.name );
			DrawString( m_hSmallFont, tx, ty + nameTall + 2, row.bIn ? Color( 200, 200, 200, 255 ) : Color( 230, 200, 80, 255 ), row.state );
			y += rowTall + pad / 2;
		}

		DrawString( m_hSmallFont, pad, y + pad / 2, Color( 170, 170, 170, 255 ), HintText() );
	}

private:
	struct Row_t
	{
		int  slot;
		bool bIn;			// Playing / ready, rather than a controller that can join
		char name[96];		// Profile - character
		char state[96];
	};

	static const char *HintText() { return "LB / RB: change profile    START: join / leave"; }

	static int IconWide( int rowTall ) { return ( rowTall - 4 ) * 3 / 2; }

	int FontTall( HFont font ) { return font != INVALID_FONT ? surface()->GetFontTall( font ) : 14; }

	static void CharacterName( const char *ident, char *out, int outLen )
	{
		const CGECharData *pChar = ident && ident[0] ? GECharacters()->Get( ident ) : NULL;
		if ( !pChar || !Q_stricmp( ident, CHAR_RANDOM_IDENT ) || pChar == GECharacters()->GetRandomChar() )
		{
			Q_strncpy( out, "Random", outLen );
			return;
		}
		Q_strncpy( out, pChar->szPrintName[0] ? pChar->szPrintName : pChar->szIdentifier, outLen );
		if ( out[0] == '#' )
		{
			const wchar_t *pw = g_pVGuiLocalize->Find( out );
			if ( pw )
				g_pVGuiLocalize->ConvertUnicodeToANSI( pw, out, outLen );
		}
	}

	// Same people as the old "Local players" text: player 1, everyone joined,
	// and any other connected controller that can press START to join
	void BuildRows()
	{
		CGESplitScreenClient *pSS = GESplitScreenClient();
		m_iRows = 0;
		for ( int slot = 0; slot < GES_SS_MAX_PLAYERS; slot++ )
		{
			const int pad = pSS->GetSlotPad( slot );
			const bool bPad = pad >= 0 && GESSGamepads()->GetPad( pad ).connected;
			const bool bEmpty = slot > 0 && pSS->GetSlotState( slot ) == GESS_SLOT_EMPTY;
			if ( bEmpty && !bPad )
				continue;

			Row_t &row = m_Rows[m_iRows++];
			row.slot = slot;
			row.bIn = !bEmpty;

			if ( bEmpty )
			{
				Q_strncpy( row.name, VarArgs( "Controller %d", pad + 1 ), sizeof( row.name ) );
				Q_strncpy( row.state, "Press START to join", sizeof( row.state ) );
				continue;
			}

			const GESSProfile_t *p = GESSProfiles()->Get( pSS->GetSlotProfile( slot ) );
			char character[64];
			CharacterName( p ? p->character : CHAR_RANDOM_IDENT, character, sizeof( character ) );
			Q_snprintf( row.name, sizeof( row.name ), "%s - %s", pSS->GetSlotName( slot ), character );

			if ( slot == 0 )
				Q_strncpy( row.state, bPad ? "Ready - LB / RB: profile" : "Ready (keyboard)", sizeof( row.state ) );
			else if ( pSS->GetSlotState( slot ) == GESS_SLOT_CHOOSING )
				Q_strncpy( row.state, "Choosing a profile", sizeof( row.state ) );
			else
				Q_strncpy( row.state, bPad ? "Ready - START: leave" : "Ready (no controller)", sizeof( row.state ) );
		}

		if ( GESSGamepads()->GetMaxPads() == 0 && m_iRows < GES_SS_MAX_PLAYERS )
		{
			Row_t &row = m_Rows[m_iRows++];
			row.slot = -1;
			row.bIn = false;
			Q_strncpy( row.name, "No controller support", sizeof( row.name ) );
			Q_strncpy( row.state, "SDL2 or XInput not found", sizeof( row.state ) );
		}
	}

	// A small controller: body, two grips, and the player number in the middle
	void DrawGamepad( int x, int y, int w, int h, int number, bool bIn, bool bHost )
	{
		if ( number <= 0 )
			return;

		const Color body = bHost ? Color( 200, 170, 70, 255 ) : ( bIn ? Color( 150, 165, 190, 255 ) : Color( 110, 110, 110, 255 ) );
		const Color dark( 20, 20, 20, 255 );

		// The silhouette: shoulders, body, and two grips hanging below the
		// outer thirds. Drawn black one pixel bigger first, so it has one outline.
		const int shoulderTop = y + 1, bodyTop = y + h / 8, bodyBottom = y + h * 5 / 8, gripMid = y + h * 13 / 16;
		const int gripW = Max( 4, w * 3 / 10 ), gripInset = Max( 1, w / 24 ), shoulderW = Max( 3, w / 4 );
		const int lowerW = Max( 3, gripW * 3 / 4 );
		struct { int x0, y0, x1, y1; } parts[] = {
			{ x + w / 10, shoulderTop, x + w / 10 + shoulderW, bodyTop + 1 },				// LB
			{ x + w - w / 10 - shoulderW, shoulderTop, x + w - w / 10, bodyTop + 1 },		// RB
			{ x, bodyTop, x + w, bodyBottom },												// body
			{ x + gripInset, bodyTop, x + gripInset + gripW, gripMid },						// left grip
			{ x + w - gripInset - gripW, bodyTop, x + w - gripInset, gripMid },				// right grip
			{ x + gripInset, gripMid - 1, x + gripInset + lowerW, y + h },					// grip ends, narrower
			{ x + w - gripInset - lowerW, gripMid - 1, x + w - gripInset, y + h },
		};
		surface()->DrawSetColor( dark );
		for ( int i = 0; i < ARRAYSIZE( parts ); i++ )
			surface()->DrawFilledRect( parts[i].x0 - 1, parts[i].y0 - 1, parts[i].x1 + 1, parts[i].y1 + 1 );
		for ( int i = 0; i < ARRAYSIZE( parts ); i++ )
		{
			// Shoulder buttons a little darker than the body
			surface()->DrawSetColor( i < 2 ? Color( body.r() * 3 / 4, body.g() * 3 / 4, body.b() * 3 / 4, 255 ) : body );
			surface()->DrawFilledRect( parts[i].x0, parts[i].y0, parts[i].x1, parts[i].y1 );
		}

		// D-pad on the left, four face buttons on the right
		const int cy = ( bodyTop + bodyBottom ) / 2;
		const int arm = Max( 2, ( bodyBottom - bodyTop ) / 4 ), thick = Max( 1, arm * 2 / 3 );
		const int lx = x + gripInset + gripW / 2, rx = x + w - gripInset - gripW / 2;
		surface()->DrawSetColor( dark );
		surface()->DrawFilledRect( lx - arm, cy - thick / 2, lx + arm + 1, cy - thick / 2 + thick );
		surface()->DrawFilledRect( lx - thick / 2, cy - arm, lx - thick / 2 + thick, cy + arm + 1 );
		const int dot = Max( 2, arm * 2 / 3 );
		surface()->DrawFilledRect( rx - dot / 2, cy - arm - dot / 2, rx - dot / 2 + dot, cy - arm - dot / 2 + dot );
		surface()->DrawFilledRect( rx - dot / 2, cy + arm - dot / 2, rx - dot / 2 + dot, cy + arm - dot / 2 + dot );
		surface()->DrawFilledRect( rx - arm - dot / 2, cy - dot / 2, rx - arm - dot / 2 + dot, cy - dot / 2 + dot );
		surface()->DrawFilledRect( rx + arm - dot / 2, cy - dot / 2, rx + arm - dot / 2 + dot, cy - dot / 2 + dot );

		// Player number in the middle
		char num[8];
		Q_snprintf( num, sizeof( num ), "%d", number );
		const int nw = TextWide( m_hNumberFont, num ), nt = FontTall( m_hNumberFont );
		DrawString( m_hNumberFont, x + ( w - nw ) / 2, cy - nt / 2, dark, num );
	}

	int TextWide( HFont font, const char *text )
	{
		if ( font == INVALID_FONT )
			return 0;
		wchar_t wbuf[128];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
		int tw, th;
		surface()->GetTextSize( font, wbuf, tw, th );
		return tw;
	}

	void DrawString( HFont font, int x, int y, Color col, const char *text )
	{
		if ( font == INVALID_FONT )
			return;
		wchar_t wbuf[128];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
		surface()->DrawSetTextFont( font );
		surface()->DrawSetTextColor( col );
		surface()->DrawSetTextPos( x, y );
		surface()->DrawPrintText( wbuf, wcslen( wbuf ) );
	}

	vgui::PHandle m_hMenu;
	HFont m_hNameFont, m_hSmallFont, m_hNumberFont;
	int   m_iFontScreenTall;
	Row_t m_Rows[GES_SS_MAX_PLAYERS + 1];
	int   m_iRows;
};

//-----------------------------------------------------------------------------
// The Split Screen menu
//-----------------------------------------------------------------------------
enum GESSMenuPage_t
{
	GESS_PAGE_PROFILES = 0,
	GESS_PAGE_MATCH,
	GESS_PAGE_COUNT
};

enum GESSNavType_t
{
	GESS_NAV_BUTTON,
	GESS_NAV_CHECK,
	GESS_NAV_COMBO,
	GESS_NAV_SLIDER,
	GESS_NAV_TEXT,		// Opens the keyboard
	GESS_NAV_NUMBER,	// Left/right add or remove one
};

struct GESSNavItem_t
{
	Panel *panel;
	GESSNavType_t type;
	int page;		// -1 = on every page
	int minVal, maxVal;
};

class CGESplitScreenMenu : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CGESplitScreenMenu, vgui::Frame );

public:
	CGESplitScreenMenu( vgui::VPANEL parent );

	void OpenPage( int page );

	virtual void Activate();
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );
	virtual void PostChildPaint();
	virtual void OnClose();

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_PTR( OnSliderMoved, "SliderMoved", panel );
	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel );
	MESSAGE_FUNC_INT( OnKeyboardDone, "KeyboardDone", accepted );
	MESSAGE_FUNC( OnOpenKeyboard, "OpenKeyboard" );
	MESSAGE_FUNC_PARAMS( OnCharacterPicked, "CharacterPicked", data );

private:
	void SetPage( int page );
	void AddNav( Panel *panel, GESSNavType_t type, int page, int minVal = 0, int maxVal = 0 );
	bool IsNavUsable( int i ) const;
	void MoveFocus( int dir );
	void ChangeFocused( int delta );
	void ActivateFocused();
	void HandlePads();
	void OpenKeyboard( TextEntry *pEntry, const char *title, int maxLen );

	// Profiles page
	void RefreshProfileList( int select );
	void LoadProfileIntoControls();
	void StoreControlsIntoProfile();
	void UpdateSensitivityLabel();
	void UpdateCharacterButton();
	const char *GetSelectedCharacter();

	// Match page
	void LoadMatchIntoControls();
	void StoreControlsIntoMatch();
	void UpdateMatchControls();
	int  GetEntryInt( TextEntry *pEntry, int fallback );

	int			m_iPage;
	bool		m_bLoading;

	// Common
	Button		*m_pTabs[GESS_PAGE_COUNT];
	Label		*m_pPadHint;
	Button		*m_pCloseButton;
	CGESSOnScreenKeyboard *m_pKeyboard;
	CGESSCharacterPicker *m_pCharPicker;
	CGESSIntroPanel *m_pIntro;

	// Profiles
	int			m_iEditing;
	ComboBox	*m_pProfileList;
	Button		*m_pNewButton;
	Button		*m_pDeleteButton;
	Label		*m_pNameLabel;
	TextEntry	*m_pName;
	Label		*m_pCharLabel;
	ComboBox	*m_pCharacter;		// Hidden: holds the choice, the picker sets it
	Button		*m_pCharButton;		// Opens the character picker
	ImagePanel	*m_pCharPortrait;
	Label		*m_pSensLabel;
	Slider		*m_pSensitivity;
	Label		*m_pSensValue;
	CheckButton	*m_pInvert;
	CheckButton	*m_pInvited;
	Label		*m_pHelp;
	Button		*m_pSaveButton;
	// The controllers' measured stick dead zones, and a button to measure again
	Button		*m_pRecalibrate;
	Label		*m_pPadCal;
	float		m_flNextPadCalText;
	void		UpdatePadCalText();
	CGESSPlayerList *m_pPlayerList;	// Top right of the screen, on every page

	// Match
	Label		*m_pMapLabel;
	ComboBox	*m_pMap;
	Label		*m_pModeLabel;
	ComboBox	*m_pMode;
	Label		*m_pWeaponsLabel;
	ComboBox	*m_pWeapons;
	CheckButton	*m_pTeams;
	CheckButton	*m_pJump;			// ge_allowjump
	CheckButton	*m_pRadar;			// ge_allowradar
	bool		m_bTeamsChoice;		// What the player picked (modes that force teams on/off don't change it)
	Label		*m_pWinLabel;
	ComboBox	*m_pWin;
	Label		*m_pTimeLabel;
	TextEntry	*m_pTime;
	Label		*m_pKillsLabel;
	TextEntry	*m_pKills;
	Label		*m_pPerDisplayLabel;
	ComboBox	*m_pPerDisplay;		// Players per display, 1 to the number of players who joined
	Label		*m_pPadCount;		// "4 players joined"
	int			m_iPerDisplayWanted;	// Player 1's choice (comes back when more players join)
	int			m_iPerDisplayLimit;		// What the list was built for
	int			m_iShownPadCount;
	int			m_iPerDisplayShown;		// The value RebuildPerDisplay put in the list
	void		RebuildPerDisplay();
	CGESSLayoutDiagram *m_pDiagram;
	Label		*m_pSummary;
	Button		*m_pStartButton;

	// Controller navigation
	CUtlVector<GESSNavItem_t> m_Nav;
	CUtlVector<Panel*> m_PagePanels[GESS_PAGE_COUNT];
	int			m_iFocus;
	bool		m_bPadFocusShown;
	CGESSMenuPadReader m_Pads;

};

static GameUI<CGESplitScreenMenu> g_GESplitScreenMenu;

CGESplitScreenMenu::CGESplitScreenMenu( vgui::VPANEL parent ) : BaseClass( NULL, "GESplitScreenMenu" )
{
	SetParent( parent );
	SetScheme( vgui::scheme()->LoadSchemeFromFile( "resource/SourceScheme.res", "SourceScheme" ) );
	SetTitle( "Split Screen", true );
	SetSizeable( false );
	SetMoveable( true );
	SetDeleteSelfOnClose( false );
	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( true );
	SetPostChildPaintEnabled( true );

	m_iPage = GESS_PAGE_PROFILES;
	m_bLoading = false;
	m_iEditing = -1;
	m_iFocus = 0;
	m_bPadFocusShown = false;

	m_pTabs[GESS_PAGE_PROFILES] = new Button( this, "TabProfiles", "Profiles", this, "page0" );
	m_pTabs[GESS_PAGE_MATCH] = new Button( this, "TabMatch", "Match", this, "page1" );

	// ---- Profiles page ----
	CUtlVector<Panel*> &prof = m_PagePanels[GESS_PAGE_PROFILES];

	m_pProfileList = new ComboBox( this, "ProfileList", 12, false );
	m_pNewButton = new Button( this, "NewButton", "New profile", this, "new" );
	m_pDeleteButton = new Button( this, "DeleteButton", "Delete", this, "delete" );

	m_pNameLabel = new Label( this, "NameLabel", "Name" );
	m_pName = new CGESSKeyboardTextEntry( this, "Name" );
	m_pName->SetMaximumCharCount( GES_SS_PROFILE_NAME_LEN - 1 );

	m_pCharLabel = new Label( this, "CharLabel", "Character" );
	m_pCharacter = new ComboBox( this, "Character", 12, false );
	m_pCharacter->SetVisible( false );
	m_pCharButton = new Button( this, "CharButton", "Random", this, "pickchar" );
	m_pCharPortrait = new ImagePanel( this, "CharPortrait" );
	m_pCharPortrait->SetShouldScaleImage( true );
	m_pCharPortrait->SetMouseInputEnabled( false );

	m_pSensLabel = new Label( this, "SensLabel", "Controller sensitivity" );
	m_pSensitivity = new Slider( this, "Sensitivity" );
	m_pSensitivity->SetRange( (int)( GES_SS_SENS_MIN * GES_SS_SENS_SLIDER_SCALE ), (int)( GES_SS_SENS_MAX * GES_SS_SENS_SLIDER_SCALE ) );
	m_pSensitivity->SetNumTicks( 10 );
	m_pSensValue = new Label( this, "SensValue", "1.00" );

	m_pInvert = new CheckButton( this, "Invert", "Invert look up/down" );
	m_pInvited = new CheckButton( this, "Invited", "Invited (offered when a player presses START to join)" );

	m_pHelp = new Label( this, "Help", "Players press START on their controller to get ready (they appear in the top right), or to join during a game. LB / RB change each player's profile." );
	m_pHelp->SetWrap( true );
	m_pHelp->SetContentAlignment( Label::a_northwest );
	m_pSaveButton = new Button( this, "SaveButton", "Save", this, "save" );
	m_pRecalibrate = new Button( this, "Recalibrate", "Recalibrate controllers", this, "recalibrate" );
	m_pPadCal = new Label( this, "PadCalibration", "" );
	m_pPadCal->SetContentAlignment( Label::a_northwest );
	m_flNextPadCalText = 0;

	// The local players: its own panel so it can sit in the screen's corner
	m_pPlayerList = new CGESSPlayerList( parent );
	m_pPlayerList->SetMenu( this );

	Panel *profPanels[] = { m_pProfileList, m_pNewButton, m_pDeleteButton, m_pNameLabel, m_pName, m_pCharLabel, m_pCharButton, m_pCharPortrait,
		m_pSensLabel, m_pSensitivity, m_pSensValue, m_pInvert, m_pInvited, m_pHelp, m_pSaveButton, m_pRecalibrate, m_pPadCal };
	for ( int i = 0; i < ARRAYSIZE( profPanels ); i++ )
		prof.AddToTail( profPanels[i] );

	// ---- Match page ----
	CUtlVector<Panel*> &match = m_PagePanels[GESS_PAGE_MATCH];

	m_pMapLabel = new Label( this, "MapLabel", "Map" );
	m_pMap = new ComboBox( this, "Map", 12, false );
	m_pModeLabel = new Label( this, "ModeLabel", "Game mode" );
	m_pMode = new ComboBox( this, "Mode", 12, false );
	m_pWeaponsLabel = new Label( this, "WeaponsLabel", "Weapons" );
	m_pWeapons = new ComboBox( this, "Weapons", 15, false );
	m_pTeams = new CheckButton( this, "Teams", "Team game (MI6 vs Janus)" );
	m_pJump = new CheckButton( this, "Jump", "Jumping allowed (right stick click)" );
	m_pRadar = new CheckButton( this, "Radar", "Radar" );

	m_pWinLabel = new Label( this, "WinLabel", "Win condition" );
	m_pWin = new ComboBox( this, "WinCondition", ARRAYSIZE( s_WinConditionNames ), false );
	for ( int i = 0; i < ARRAYSIZE( s_WinConditionNames ); i++ )
		m_pWin->AddItem( s_WinConditionNames[i], new KeyValues( "win", "value", i ) );

	m_pTimeLabel = new Label( this, "TimeLabel", "Time limit (minutes)" );
	m_pTime = new TextEntry( this, "TimeLimit" );
	m_pTime->SetAllowNumericInputOnly( true );
	m_pTime->SetMaximumCharCount( 2 );
	m_pKillsLabel = new Label( this, "KillsLabel", "Kills to win" );
	m_pKills = new TextEntry( this, "KillLimit" );
	m_pKills->SetAllowNumericInputOnly( true );
	m_pKills->SetMaximumCharCount( 3 );

	m_pPerDisplayLabel = new Label( this, "PerDisplayLabel", "Players per display" );
	m_pPerDisplay = new ComboBox( this, "PerDisplay", GES_SS_MAX_PLAYERS, false );	// 1 to the players who joined, RebuildPerDisplay
	m_pPadCount = new Label( this, "PadCount", "" );
	m_iPerDisplayWanted = 4;
	m_iPerDisplayLimit = -1;
	m_iShownPadCount = -1;
	m_iPerDisplayShown = -1;
	m_pDiagram = new CGESSLayoutDiagram( this );

	m_pSummary = new Label( this, "Summary", "" );
	m_pSummary->SetWrap( true );
	m_pSummary->SetContentAlignment( Label::a_northwest );
	m_pStartButton = new Button( this, "StartButton", "Start match", this, "start" );

	Panel *matchPanels[] = { m_pMapLabel, m_pMap, m_pModeLabel, m_pMode, m_pWeaponsLabel, m_pWeapons, m_pTeams, m_pJump, m_pRadar, m_pWinLabel, m_pWin,
		m_pTimeLabel, m_pTime, m_pKillsLabel, m_pKills, m_pPerDisplayLabel, m_pPerDisplay, m_pPadCount, m_pDiagram, m_pSummary, m_pStartButton };
	for ( int i = 0; i < ARRAYSIZE( matchPanels ); i++ )
		match.AddToTail( matchPanels[i] );

	// ---- Common ----
	m_pPadHint = new Label( this, "PadHint", "D-pad: move   Left/Right: change   A: select   B: close   Y: Profiles / Match   START: join   LB/RB: your profile" );
	m_pPadHint->SetContentAlignment( Label::a_center );
	m_pCloseButton = new Button( this, "CloseButton", "Close", this, "close" );

	m_pKeyboard = new CGESSOnScreenKeyboard( this );
	m_pCharPicker = new CGESSCharacterPicker( this );
	m_pCharPicker->AddActionSignalTarget( this );
	m_pIntro = new CGESSIntroPanel( this );
	m_pKeyboard->AddActionSignalTarget( this );

	// Every control reports changes to us
	Panel *signalPanels[] = { m_pProfileList, m_pName, m_pCharacter, m_pSensitivity, m_pInvert, m_pInvited,
		m_pMap, m_pMode, m_pWeapons, m_pTeams, m_pJump, m_pRadar, m_pWin, m_pTime, m_pKills, m_pPerDisplay };
	for ( int i = 0; i < ARRAYSIZE( signalPanels ); i++ )
		signalPanels[i]->AddActionSignalTarget( this );

	// Controller navigation order, top to bottom
	AddNav( m_pTabs[GESS_PAGE_PROFILES], GESS_NAV_BUTTON, -1 );
	AddNav( m_pTabs[GESS_PAGE_MATCH], GESS_NAV_BUTTON, -1 );
	AddNav( m_pProfileList, GESS_NAV_COMBO, GESS_PAGE_PROFILES );
	AddNav( m_pNewButton, GESS_NAV_BUTTON, GESS_PAGE_PROFILES );
	AddNav( m_pDeleteButton, GESS_NAV_BUTTON, GESS_PAGE_PROFILES );
	AddNav( m_pName, GESS_NAV_TEXT, GESS_PAGE_PROFILES );
	AddNav( m_pCharButton, GESS_NAV_BUTTON, GESS_PAGE_PROFILES );
	AddNav( m_pSensitivity, GESS_NAV_SLIDER, GESS_PAGE_PROFILES );
	AddNav( m_pInvert, GESS_NAV_CHECK, GESS_PAGE_PROFILES );
	AddNav( m_pInvited, GESS_NAV_CHECK, GESS_PAGE_PROFILES );
	AddNav( m_pRecalibrate, GESS_NAV_BUTTON, GESS_PAGE_PROFILES );
	AddNav( m_pSaveButton, GESS_NAV_BUTTON, GESS_PAGE_PROFILES );
	AddNav( m_pMap, GESS_NAV_COMBO, GESS_PAGE_MATCH );
	AddNav( m_pMode, GESS_NAV_COMBO, GESS_PAGE_MATCH );
	AddNav( m_pWeapons, GESS_NAV_COMBO, GESS_PAGE_MATCH );
	AddNav( m_pTeams, GESS_NAV_CHECK, GESS_PAGE_MATCH );
	AddNav( m_pJump, GESS_NAV_CHECK, GESS_PAGE_MATCH );
	AddNav( m_pRadar, GESS_NAV_CHECK, GESS_PAGE_MATCH );
	AddNav( m_pWin, GESS_NAV_COMBO, GESS_PAGE_MATCH );
	AddNav( m_pTime, GESS_NAV_NUMBER, GESS_PAGE_MATCH, 0, GESS_MATCH_TIME_MAX );
	AddNav( m_pKills, GESS_NAV_NUMBER, GESS_PAGE_MATCH, 1, GESS_MATCH_KILLS_MAX );
	AddNav( m_pPerDisplay, GESS_NAV_COMBO, GESS_PAGE_MATCH );
	AddNav( m_pStartButton, GESS_NAV_BUTTON, GESS_PAGE_MATCH );
	AddNav( m_pCloseButton, GESS_NAV_BUTTON, -1 );

	SetSize( 600, 540 );
	SetVisible( false );
}

void CGESplitScreenMenu::AddNav( Panel *panel, GESSNavType_t type, int page, int minVal, int maxVal )
{
	GESSNavItem_t item = { panel, type, page, minVal, maxVal };
	m_Nav.AddToTail( item );
}

void CGESplitScreenMenu::OpenPage( int page )
{
	if ( !IsVisible() )
		Activate();
	SetPage( page );
}

void CGESplitScreenMenu::Activate()
{
	const bool bWasVisible = IsVisible();
	BaseClass::Activate();
	if ( bWasVisible )
		return;

	// First time: how split screen is played (until "Don't show this again")
	if ( !ges_ss_hide_intro.GetBool() )
		m_pIntro->Open();
	else if ( m_pIntro->IsOpen() )
		m_pIntro->SetVisible( false );

	m_bLoading = true;

	// Characters: random first, then everyone
	GECharacters_EnsureParsed();
	m_pCharacter->RemoveAll();
	m_pCharacter->AddItem( "Random", new KeyValues( "char", "ident", CHAR_RANDOM_IDENT ) );
	for ( int i = 0; i < GECharacters()->Count(); i++ )
	{
		const CGECharData *pChar = GECharacters()->Get( i );
		if ( !pChar || !Q_stricmp( pChar->szIdentifier, CHAR_RANDOM_IDENT ) )
			continue;
		m_pCharacter->AddItem( pChar->szPrintName[0] ? pChar->szPrintName : pChar->szIdentifier, new KeyValues( "char", "ident", pChar->szIdentifier ) );
	}

	// Maps and game modes
	GESSMatch()->Load();
	m_pMap->RemoveAll();
	CUtlVector<CUtlString> maps;
	GESSMatch()->RefreshMaps( maps );
	for ( int i = 0; i < maps.Count(); i++ )
		m_pMap->AddItem( maps[i].Get(), new KeyValues( "map", "name", maps[i].Get() ) );

	m_pMode->RemoveAll();
	for ( int i = 0; i < GESSMatch()->GetModeCount(); i++ )
	{
		const GESSGameMode_t &mode = GESSMatch()->GetMode( i );
		m_pMode->AddItem( mode.name, new KeyValues( "mode", "ident", mode.ident ) );
	}

	// Weapon sets, like the N64's weapon options (Power Weapons, Proximity Mines...)
	m_pWeapons->RemoveAll();
	for ( int i = 0; i < GESSMatch()->GetWeaponSetCount(); i++ )
	{
		const GESSWeaponSet_t &set = GESSMatch()->GetWeaponSet( i );
		m_pWeapons->AddItem( set.name, new KeyValues( "weapons", "ident", set.ident ) );
	}

	m_bLoading = false;

	GESSProfiles()->Load();
	if ( GESSProfiles()->Count() == 0 )
		GESSProfiles()->Add();

	RefreshProfileList( 0 );
	LoadMatchIntoControls();
	SetPage( m_iPage );

	m_Pads.Reset();
	m_bPadFocusShown = GESSGamepads()->GetConnectedCount() > 0;
	MoveToCenterOfScreen();
}

void CGESplitScreenMenu::OnClose()
{
	if ( m_pKeyboard->IsOpen() )
		m_pKeyboard->SetVisible( false );
	if ( m_pCharPicker->IsOpen() )
		m_pCharPicker->SetVisible( false );

	StoreControlsIntoProfile();
	GESSProfiles()->Save();
	StoreControlsIntoMatch();
	GESSMatch()->Save();

	BaseClass::OnClose();
}

void CGESplitScreenMenu::SetPage( int page )
{
	m_iPage = clamp( page, 0, GESS_PAGE_COUNT - 1 );

	for ( int p = 0; p < GESS_PAGE_COUNT; p++ )
	{
		for ( int i = 0; i < m_PagePanels[p].Count(); i++ )
			m_PagePanels[p][i]->SetVisible( p == m_iPage );

		m_pTabs[p]->ForceDepressed( p == m_iPage );
	}

	// Start on the first setting of the page
	m_iFocus = 0;
	for ( int i = 0; i < m_Nav.Count(); i++ )
	{
		if ( m_Nav[i].page == m_iPage && IsNavUsable( i ) )
		{
			m_iFocus = i;
			break;
		}
	}

	InvalidateLayout();
	Repaint();
}

void CGESplitScreenMenu::PerformLayout()
{
	BaseClass::PerformLayout();

	const int x = 20, labelW = 170, fieldX = x + labelW, fieldW = GetWide() - fieldX - 20;
	const int W = GetWide(), H = GetTall();

	m_pTabs[GESS_PAGE_PROFILES]->SetBounds( x, 36, 120, 26 );
	m_pTabs[GESS_PAGE_MATCH]->SetBounds( x + 126, 36, 120, 26 );

	// ---- Profiles ----
	int y = 76;
	m_pProfileList->SetBounds( x, y, W - 40 - 200, 24 );
	m_pNewButton->SetBounds( W - 20 - 190, y, 110, 24 );
	m_pDeleteButton->SetBounds( W - 20 - 70, y, 70, 24 );
	y += 40;
	m_pNameLabel->SetBounds( x, y, labelW, 24 );
	m_pName->SetBounds( fieldX, y, fieldW, 24 );
	y += 32;
	// Character: the chosen portrait and a button that opens the picker
	m_pCharLabel->SetBounds( x, y + 12, labelW, 24 );
	m_pCharPortrait->SetBounds( fieldX, y, 48, 48 );
	m_pCharButton->SetBounds( fieldX + 56, y + 12, fieldW - 56, 24 );
	y += 56;
	m_pSensLabel->SetBounds( x, y, labelW, 24 );
	m_pSensitivity->SetBounds( fieldX, y, fieldW - 50, 28 );
	m_pSensValue->SetBounds( fieldX + fieldW - 44, y, 44, 24 );
	y += 36;
	m_pInvert->SetBounds( fieldX - 4, y, fieldW, 24 );
	y += 28;
	m_pInvited->SetBounds( fieldX - 4, y, fieldW, 24 );
	y += 40;
	m_pHelp->SetBounds( x, y, W - 40, 44 );
	y += 48;
	// Controllers' stick dead zones (measured automatically)
	m_pRecalibrate->SetBounds( x, y, 190, 24 );
	m_pPadCal->SetBounds( x, y + 30, W - 40, H - 76 - ( y + 30 ) );

	// ---- Match ----
	y = 76;
	m_pMapLabel->SetBounds( x, y, labelW, 24 );
	m_pMap->SetBounds( fieldX, y, fieldW, 24 );
	y += 32;
	m_pModeLabel->SetBounds( x, y, labelW, 24 );
	m_pMode->SetBounds( fieldX, y, fieldW, 24 );
	y += 32;
	m_pWeaponsLabel->SetBounds( x, y, labelW, 24 );
	m_pWeapons->SetBounds( fieldX, y, fieldW, 24 );
	y += 30;
	m_pTeams->SetBounds( fieldX - 4, y, fieldW, 24 );
	y += 28;
	m_pJump->SetBounds( fieldX - 4, y, fieldW / 2 + 40, 24 );
	m_pRadar->SetBounds( fieldX - 4 + fieldW / 2 + 50, y, fieldW / 2 - 50, 24 );
	y += 36;
	m_pWinLabel->SetBounds( x, y, labelW, 24 );
	m_pWin->SetBounds( fieldX, y, fieldW, 24 );
	y += 32;
	m_pTimeLabel->SetBounds( x, y, labelW, 24 );
	m_pTime->SetBounds( fieldX, y, 60, 24 );
	y += 32;
	m_pKillsLabel->SetBounds( x, y, labelW, 24 );
	m_pKills->SetBounds( fieldX, y, 60, 24 );
	y += 32;
	m_pPerDisplayLabel->SetBounds( x, y, labelW, 24 );
	m_pPerDisplay->SetBounds( fieldX, y, 60, 24 );
	m_pPadCount->SetBounds( fieldX + 70, y, fieldW - 70, 24 );
	y += 36;
	// What the match is, and a picture of how the screens will be split
	m_pSummary->SetBounds( x, y, ( W - 40 ) / 2 - 10, H - y - 80 );
	m_pDiagram->SetBounds( x + ( W - 40 ) / 2, y, ( W - 40 ) / 2, H - y - 80 );

	// ---- Bottom ----
	m_pPadHint->SetBounds( x, H - 70, W - 40, 20 );
	m_pSaveButton->SetBounds( W - 20 - 170, H - 40, 80, 26 );
	m_pStartButton->SetBounds( W - 20 - 210, H - 40, 120, 26 );
	m_pCloseButton->SetBounds( W - 20 - 80, H - 40, 80, 26 );

	// The keyboard covers the page area
	m_pKeyboard->SetBounds( 14, 70, W - 28, H - 146 );	// stops above the controller hint
	m_pCharPicker->SetBounds( 14, 70, W - 28, H - 146 );

	// The introduction covers the whole menu
	m_pIntro->SetBounds( 0, 0, W, H );
}

//-----------------------------------------------------------------------------
// Controller navigation
//-----------------------------------------------------------------------------
bool CGESplitScreenMenu::IsNavUsable( int i ) const
{
	const GESSNavItem_t &item = m_Nav[i];
	if ( item.page != -1 && item.page != m_iPage )
		return false;
	return item.panel->IsVisible() && item.panel->IsEnabled();
}

void CGESplitScreenMenu::MoveFocus( int dir )
{
	const int count = m_Nav.Count();
	for ( int step = 1; step <= count; step++ )
	{
		const int i = ( m_iFocus + dir * step + count * 2 ) % count;
		if ( IsNavUsable( i ) )
		{
			m_iFocus = i;
			break;
		}
	}
	Repaint();
}

static int GESS_ComboRow( ComboBox *pCombo )
{
	const int active = pCombo->GetActiveItem();
	for ( int row = 0; row < pCombo->GetItemCount(); row++ )
	{
		if ( pCombo->GetItemIDFromRow( row ) == active )
			return row;
	}
	return 0;
}

void CGESplitScreenMenu::ChangeFocused( int delta )
{
	if ( !m_Nav.IsValidIndex( m_iFocus ) || !IsNavUsable( m_iFocus ) )
		return;

	const GESSNavItem_t &item = m_Nav[m_iFocus];
	switch ( item.type )
	{
	case GESS_NAV_COMBO:
		{
			ComboBox *pCombo = static_cast<ComboBox*>( item.panel );
			const int count = pCombo->GetItemCount();
			if ( count > 0 )
				pCombo->ActivateItemByRow( ( GESS_ComboRow( pCombo ) + delta + count ) % count );
		}
		break;

	case GESS_NAV_SLIDER:
		{
			Slider *pSlider = static_cast<Slider*>( item.panel );
			int lo, hi;
			pSlider->GetRange( lo, hi );
			pSlider->SetValue( clamp( pSlider->GetValue() + delta * 5, lo, hi ) );	// 0.05 per press
		}
		break;

	case GESS_NAV_NUMBER:
		{
			TextEntry *pEntry = static_cast<TextEntry*>( item.panel );
			const int value = clamp( GetEntryInt( pEntry, item.minVal ) + delta, item.minVal, item.maxVal );
			pEntry->SetText( VarArgs( "%d", value ) );
		}
		break;

	case GESS_NAV_CHECK:
		{
			CheckButton *pCheck = static_cast<CheckButton*>( item.panel );
			pCheck->SetSelected( !pCheck->IsSelected() );
		}
		break;

	case GESS_NAV_BUTTON:
		// Left/right on the tabs or the bottom buttons moves sideways
		MoveFocus( delta );
		break;

	default:
		break;
	}
}

void CGESplitScreenMenu::ActivateFocused()
{
	if ( !m_Nav.IsValidIndex( m_iFocus ) || !IsNavUsable( m_iFocus ) )
		return;

	const GESSNavItem_t &item = m_Nav[m_iFocus];
	switch ( item.type )
	{
	case GESS_NAV_BUTTON:
		static_cast<Button*>( item.panel )->DoClick();
		break;
	case GESS_NAV_CHECK:
		{
			CheckButton *pCheck = static_cast<CheckButton*>( item.panel );
			pCheck->SetSelected( !pCheck->IsSelected() );
		}
		break;
	case GESS_NAV_COMBO:
		ChangeFocused( 1 );
		break;
	case GESS_NAV_TEXT:
		OpenKeyboard( static_cast<TextEntry*>( item.panel ), "Profile name", GES_SS_PROFILE_NAME_LEN - 1 );
		break;
	case GESS_NAV_NUMBER:
		MoveFocus( 1 );
		break;
	default:
		break;
	}
}

void CGESplitScreenMenu::HandlePads()
{
	m_Pads.Update();
	if ( !m_Pads.Any() )
		return;

	m_bPadFocusShown = true;

	if ( m_pIntro->IsOpen() )
	{
		m_pIntro->HandlePads( m_Pads );
		return;
	}

	if ( m_pKeyboard->IsOpen() )
	{
		m_pKeyboard->HandlePads( m_Pads );
		return;
	}

	if ( m_pCharPicker->IsOpen() )
	{
		m_pCharPicker->HandlePads( m_Pads );
		return;
	}

	if ( m_Pads.Dir( GESS_DIR_UP ) )		MoveFocus( -1 );
	if ( m_Pads.Dir( GESS_DIR_DOWN ) )		MoveFocus( 1 );
	if ( m_Pads.Dir( GESS_DIR_LEFT ) )		ChangeFocused( -1 );
	if ( m_Pads.Dir( GESS_DIR_RIGHT ) )		ChangeFocused( 1 );

	// LB / RB choose each player's profile in the lobby (ge_splitscreen_client.cpp),
	// so Y switches pages
	if ( m_Pads.Pressed( GESS_PAD_Y ) )		SetPage( ( m_iPage + 1 ) % GESS_PAGE_COUNT );

	if ( m_Pads.Pressed( GESS_PAD_A ) )
		ActivateFocused();
	else if ( m_Pads.Pressed( GESS_PAD_B ) )
		Close();
}

void CGESplitScreenMenu::PostChildPaint()
{
	BaseClass::PostChildPaint();

	if ( !m_bPadFocusShown || m_pIntro->IsOpen() || m_pKeyboard->IsOpen() || m_pCharPicker->IsOpen() || !m_Nav.IsValidIndex( m_iFocus ) || !IsNavUsable( m_iFocus ) )
		return;

	// Gold outline around the setting the controller is on
	int x, y, w, h;
	m_Nav[m_iFocus].panel->GetBounds( x, y, w, h );
	surface()->DrawSetColor( Color( 230, 200, 80, 255 ) );
	surface()->DrawOutlinedRect( x - 3, y - 3, x + w + 3, y + h + 3 );
	surface()->DrawOutlinedRect( x - 2, y - 2, x + w + 2, y + h + 2 );
}

void CGESplitScreenMenu::OpenKeyboard( TextEntry *pEntry, const char *title, int maxLen )
{
	m_pKeyboard->Open( pEntry, title, maxLen );
}

void CGESplitScreenMenu::OnOpenKeyboard()
{
	if ( !m_pKeyboard->IsOpen() && m_pName->IsEnabled() )
		OpenKeyboard( m_pName, "Profile name", GES_SS_PROFILE_NAME_LEN - 1 );
}

void CGESplitScreenMenu::OnKeyboardDone( int accepted )
{
	m_Pads.Reset();
	StoreControlsIntoProfile();
	if ( accepted )
		RefreshProfileList( m_iEditing );
}

// The character ident the (hidden) character list has selected
const char *CGESplitScreenMenu::GetSelectedCharacter()
{
	KeyValues *pData = m_pCharacter->GetActiveItemUserData();
	return pData ? pData->GetString( "ident", CHAR_RANDOM_IDENT ) : CHAR_RANDOM_IDENT;
}

void CGESplitScreenMenu::OnCharacterPicked( KeyValues *data )
{
	m_Pads.Reset();
	if ( !data->GetInt( "accepted" ) )
		return;

	const char *ident = data->GetString( "ident" );
	for ( int i = 0; i < m_pCharacter->GetItemCount(); i++ )
	{
		KeyValues *pData = m_pCharacter->GetItemUserData( m_pCharacter->GetItemIDFromRow( i ) );
		if ( pData && !Q_stricmp( pData->GetString( "ident" ), ident ) )
		{
			m_pCharacter->ActivateItemByRow( i );
			break;
		}
	}

	StoreControlsIntoProfile();
	UpdateCharacterButton();
}

// Show the chosen character's portrait and name on the profile page
void CGESplitScreenMenu::UpdateCharacterButton()
{
	const char *ident = GetSelectedCharacter();
	const CGECharData *pChar = GECharacters()->Get( ident );
	const bool bRandom = !pChar || !Q_stricmp( ident, CHAR_RANDOM_IDENT ) || pChar == GECharacters()->GetRandomChar();

	char name[128];
	Q_strncpy( name, bRandom ? "Random" : pChar->szPrintName, sizeof( name ) );
	if ( !bRandom && name[0] == '#' )
	{
		const wchar_t *pw = g_pVGuiLocalize->Find( name );
		if ( pw )
			g_pVGuiLocalize->ConvertUnicodeToANSI( pw, name, sizeof( name ) );
	}
	m_pCharButton->SetText( VarArgs( "%s  (choose...)", name ) );

	char image[128];
	Q_snprintf( image, sizeof( image ), "charselect/%s_portrait_N", pChar ? pChar->szIdentifier : CHAR_RANDOM_IDENT );
	m_pCharPortrait->SetImage( image );
}

// One line per controller: its stick dead zones (and drift) and how far each reaches
void CGESplitScreenMenu::UpdatePadCalText()
{
	if ( !m_pPadCal )
		return;

	static ConVarRef ges_ss_pad_deadzone_auto( "ges_ss_pad_deadzone_auto" );
	if ( ges_ss_pad_deadzone_auto.IsValid() && !ges_ss_pad_deadzone_auto.GetBool() )
	{
		m_pPadCal->SetText( "Automatic dead zones are off (ges_ss_pad_deadzone_auto 0)." );
		return;
	}

	char szText[1024] = "";
	CGESSGamepads *pPads = GESSGamepads();
	for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
	{
		if ( !pPads->GetPad( i ).connected )
			continue;
		char szLine[256];
		char szName[24];
		Q_strncpy( szName, pPads->GetPadName( i ), sizeof( szName ) );
		int len = Q_snprintf( szLine, sizeof( szLine ), "%d. %s:", i + 1, szName );
		for ( int s = 0; s < 2; s++ )
		{
			const CGESSGamepads::StickCal_t &sc = pPads->GetStickCal( i, s );
			if ( sc.measured )
				len += Q_snprintf( szLine + len, sizeof( szLine ) - len, "   %s %.2f (drift %.2f, reach %.2f)", s == 0 ? "left" : "right",
					sc.inner, sc.noise, pPads->GetStickOuter( i, s ) );
			else
				len += Q_snprintf( szLine + len, sizeof( szLine ) - len, "   %s measuring...", s == 0 ? "left" : "right" );
		}
		Q_strncat( szLine, "\n", sizeof( szLine ), COPY_ALL_CHARACTERS );
		Q_strncat( szText, szLine, sizeof( szText ), COPY_ALL_CHARACTERS );
	}
	if ( !szText[0] )
		Q_strncpy( szText, "No controllers connected.", sizeof( szText ) );
	else
		Q_strncat( szText, "Measured while the sticks rest in the menus. Push each stick all the way round once to set its reach.", sizeof( szText ), COPY_ALL_CHARACTERS );
	m_pPadCal->SetText( szText );
}

void CGESplitScreenMenu::OnThink()
{
	BaseClass::OnThink();

	HandlePads();

	if ( gpGlobals->realtime >= m_flNextPadCalText )
	{
		m_flNextPadCalText = gpGlobals->realtime + 0.5f;
		UpdatePadCalText();
	}

	// A player joined or left: players per display follows
	if ( GESS_PerDisplayLimit() != m_iPerDisplayLimit || ( m_pPadCount && GESS_MenuPadCount() != m_iShownPadCount ) )
	{
		m_iShownPadCount = GESS_MenuPadCount();
		RebuildPerDisplay();
		UpdateMatchControls();
	}
}

//-----------------------------------------------------------------------------
// Profiles page
//-----------------------------------------------------------------------------
void CGESplitScreenMenu::RefreshProfileList( int select )
{
	m_bLoading = true;

	m_pProfileList->RemoveAll();
	for ( int i = 0; i < GESSProfiles()->Count(); i++ )
	{
		const GESSProfile_t *p = GESSProfiles()->Get( i );
		m_pProfileList->AddItem( VarArgs( "%d. %s%s", i + 1, p->name, p->invited ? "" : " (not invited)" ), new KeyValues( "profile", "index", i ) );
	}

	m_iEditing = clamp( select, 0, GESSProfiles()->Count() - 1 );
	if ( m_iEditing >= 0 )
		m_pProfileList->ActivateItemByRow( m_iEditing );

	m_pNewButton->SetEnabled( GESSProfiles()->Count() < GES_SS_MAX_PROFILES );
	m_pDeleteButton->SetEnabled( GESSProfiles()->Count() > 0 );

	m_bLoading = false;
	LoadProfileIntoControls();
}

void CGESplitScreenMenu::LoadProfileIntoControls()
{
	const GESSProfile_t *p = GESSProfiles()->Get( m_iEditing );
	const bool bHave = p != NULL;

	m_pName->SetEnabled( bHave );
	m_pCharacter->SetEnabled( bHave );
	m_pCharButton->SetEnabled( bHave );
	m_pSensitivity->SetEnabled( bHave );
	m_pInvert->SetEnabled( bHave );
	m_pInvited->SetEnabled( bHave );
	if ( !p )
		return;

	m_bLoading = true;

	m_pName->SetText( p->name );

	int charRow = 0;
	for ( int i = 0; i < m_pCharacter->GetItemCount(); i++ )
	{
		KeyValues *pData = m_pCharacter->GetItemUserData( m_pCharacter->GetItemIDFromRow( i ) );
		if ( pData && !Q_stricmp( pData->GetString( "ident" ), p->character ) )
		{
			charRow = i;
			break;
		}
	}
	m_pCharacter->ActivateItemByRow( charRow );
	UpdateCharacterButton();

	m_pSensitivity->SetValue( (int)( p->sensitivity * GES_SS_SENS_SLIDER_SCALE + 0.5f ) );
	m_pInvert->SetSelected( p->invertY );
	m_pInvited->SetSelected( p->invited );
	UpdateSensitivityLabel();

	m_bLoading = false;
}

void CGESplitScreenMenu::StoreControlsIntoProfile()
{
	GESSProfile_t *p = GESSProfiles()->Get( m_iEditing );
	if ( !p || m_bLoading )
		return;

	char szName[GES_SS_PROFILE_NAME_LEN];
	m_pName->GetText( szName, sizeof( szName ) );
	if ( szName[0] )
		Q_strncpy( p->name, szName, sizeof( p->name ) );

	KeyValues *pData = m_pCharacter->GetActiveItemUserData();
	if ( pData )
		Q_strncpy( p->character, pData->GetString( "ident", CHAR_RANDOM_IDENT ), sizeof( p->character ) );

	p->sensitivity = clamp( m_pSensitivity->GetValue() / GES_SS_SENS_SLIDER_SCALE, GES_SS_SENS_MIN, GES_SS_SENS_MAX );
	p->invertY = m_pInvert->IsSelected();
	p->invited = m_pInvited->IsSelected();
}

void CGESplitScreenMenu::UpdateSensitivityLabel()
{
	m_pSensValue->SetText( VarArgs( "%.2f", m_pSensitivity->GetValue() / GES_SS_SENS_SLIDER_SCALE ) );
}

//-----------------------------------------------------------------------------
// Match page
//-----------------------------------------------------------------------------
int CGESplitScreenMenu::GetEntryInt( TextEntry *pEntry, int fallback )
{
	char text[16];
	pEntry->GetText( text, sizeof( text ) );
	return text[0] ? Q_atoi( text ) : fallback;
}

void CGESplitScreenMenu::LoadMatchIntoControls()
{
	const GESSMatchSettings_t &set = GESSMatch()->Settings();
	m_bLoading = true;

	int row = 0;
	for ( int i = 0; i < m_pMap->GetItemCount(); i++ )
	{
		KeyValues *pData = m_pMap->GetItemUserData( m_pMap->GetItemIDFromRow( i ) );
		if ( pData && !Q_stricmp( pData->GetString( "name" ), set.map ) )
			row = i;
	}
	if ( m_pMap->GetItemCount() > 0 )
		m_pMap->ActivateItemByRow( row );

	row = max( GESSMatch()->FindMode( set.mode ), 0 );
	if ( m_pMode->GetItemCount() > 0 )
		m_pMode->ActivateItemByRow( row );

	row = 0;
	for ( int i = 0; i < m_pWeapons->GetItemCount(); i++ )
	{
		KeyValues *pData = m_pWeapons->GetItemUserData( m_pWeapons->GetItemIDFromRow( i ) );
		if ( pData && !Q_stricmp( pData->GetString( "ident" ), set.weaponset ) )
			row = i;
	}
	if ( m_pWeapons->GetItemCount() > 0 )
		m_pWeapons->ActivateItemByRow( row );

	m_pTeams->SetSelected( set.teams );
	m_bTeamsChoice = set.teams;
	m_pJump->SetSelected( set.jump );
	m_pRadar->SetSelected( set.radar );
	m_pWin->ActivateItemByRow( clamp( set.wincondition, 0, ARRAYSIZE( s_WinConditionNames ) - 1 ) );
	m_pTime->SetText( VarArgs( "%d", set.timelimit ) );
	m_pKills->SetText( VarArgs( "%d", set.killlimit ) );
	m_iPerDisplayWanted = clamp( set.perdisplay, 1, GES_SS_MAX_PLAYERS );
	m_iPerDisplayLimit = -1;
	RebuildPerDisplay();

	m_bLoading = false;
	UpdateMatchControls();
}

// Players per display: 1 up to the number of players who joined. Player 1's
// choice comes back when more players join.
void CGESplitScreenMenu::RebuildPerDisplay()
{
	const int limit = GESS_PerDisplayLimit();
	const int value = clamp( m_iPerDisplayWanted, 1, limit );
	const bool bWasLoading = m_bLoading;
	m_bLoading = true;

	if ( limit != m_iPerDisplayLimit )
	{
		m_iPerDisplayLimit = limit;
		m_pPerDisplay->RemoveAll();
		for ( int i = 1; i <= limit; i++ )
			m_pPerDisplay->AddItem( VarArgs( "%d", i ), new KeyValues( "pd", "value", i ) );
	}
	m_pPerDisplay->ActivateItemByRow( value - 1 );
	m_iPerDisplayShown = value;
	GESSMatch()->Settings().perdisplay = value;

	const int pads = GESS_MenuPadCount();
	m_pPadCount->SetText( pads == 1 ? "1 player joined" : VarArgs( "%d players joined", pads ) );

	m_bLoading = bWasLoading;
	m_pDiagram->Repaint();
}

void CGESplitScreenMenu::StoreControlsIntoMatch()
{
	if ( m_bLoading )
		return;

	GESSMatchSettings_t &set = GESSMatch()->Settings();

	KeyValues *pData = m_pMap->GetActiveItemUserData();
	if ( pData )
		Q_strncpy( set.map, pData->GetString( "name" ), sizeof( set.map ) );

	pData = m_pMode->GetActiveItemUserData();
	if ( pData )
		Q_strncpy( set.mode, pData->GetString( "ident" ), sizeof( set.mode ) );

	pData = m_pWeapons->GetActiveItemUserData();
	if ( pData )
		Q_strncpy( set.weaponset, pData->GetString( "ident" ), sizeof( set.weaponset ) );

	pData = m_pWin->GetActiveItemUserData();
	if ( pData )
		set.wincondition = pData->GetInt( "value" );

	set.teams = m_pTeams->IsSelected();
	set.jump = m_pJump->IsSelected();
	set.radar = m_pRadar->IsSelected();
	set.timelimit = GetEntryInt( m_pTime, set.timelimit );
	set.killlimit = GetEntryInt( m_pKills, set.killlimit );

	pData = m_pPerDisplay->GetActiveItemUserData();
	if ( pData )
		set.perdisplay = pData->GetInt( "value", set.perdisplay );
}

void CGESplitScreenMenu::UpdateMatchControls()
{
	const GESSMatchSettings_t &set = GESSMatch()->Settings();

	// Teams can only be toggled for modes that support both
	const int mode = GESSMatch()->FindMode( set.mode );
	const GESSTeamSupport_t teams = mode >= 0 ? GESSMatch()->GetMode( mode ).teams : GESS_TEAMS_OPTIONAL;
	m_pTeams->SetEnabled( teams == GESS_TEAMS_OPTIONAL );

	// Modes that hand out their own weapons ignore the weapon set
	const bool bOwnWeapons = mode >= 0 && GESSMatch()->GetMode( mode ).ownWeapons;
	m_pWeapons->SetEnabled( !bOwnWeapons );
	m_pWeaponsLabel->SetEnabled( !bOwnWeapons );
	if ( teams != GESS_TEAMS_OPTIONAL && m_pTeams->IsSelected() != ( teams == GESS_TEAMS_ALWAYS ) )
	{
		m_bLoading = true;
		m_pTeams->SetSelected( teams == GESS_TEAMS_ALWAYS );
		m_bLoading = false;
	}
	else if ( teams == GESS_TEAMS_OPTIONAL && m_pTeams->IsSelected() != m_bTeamsChoice )
	{
		// Back on a mode where teams are optional: the player's own choice again
		m_bLoading = true;
		m_pTeams->SetSelected( m_bTeamsChoice );
		m_bLoading = false;
		GESSMatch()->Settings().teams = m_bTeamsChoice;
	}

	m_pKills->SetEnabled( set.wincondition == 1 );
	m_pKillsLabel->SetEnabled( set.wincondition == 1 );

	// Describe the match in plain words
	const bool bTeams = m_pTeams->IsSelected();
	const int time = GetEntryInt( m_pTime, set.timelimit );
	const int kills = GetEntryInt( m_pKills, set.killlimit );
	char summary[512];
	switch ( set.wincondition )
	{
	case 1:
		if ( time > 0 )
			Q_snprintf( summary, sizeof( summary ), "The first %s to %d kills wins. If nobody gets there in %d minutes, the most kills wins.",
				bTeams ? "team" : "player", kills, time );
		else
			Q_snprintf( summary, sizeof( summary ), "The first %s to %d kills wins. No time limit (set 0 minutes).",
				bTeams ? "team" : "player", kills );
		break;
	case 2:
		Q_snprintf( summary, sizeof( summary ), "After %d minutes the %s with the best kill/death ratio wins. Kills of teammates and suicides don't count as kills.",
			max( time, 1 ), bTeams ? "team" : "player" );
		break;
	default:
		Q_snprintf( summary, sizeof( summary ), "After %d minutes the %s with the highest score wins, using the game mode's own scoring.",
			max( time, 1 ), bTeams ? "team" : "player" );
		break;
	}
	if ( bOwnWeapons )
		Q_strncat( summary, " This mode chooses its own weapons.", sizeof( summary ), COPY_ALL_CHARACTERS );
	else if ( KeyValues *pWeapons = m_pWeapons->GetActiveItemUserData() )
	{
		char text[128];
		m_pWeapons->GetText( text, sizeof( text ) );
		if ( Q_stricmp( pWeapons->GetString( "ident" ), GESS_WEAPONSET_RANDOM ) )
			Q_strncat( summary, VarArgs( " Weapons: %s.", text ), sizeof( summary ), COPY_ALL_CHARACTERS );
		else
			Q_strncat( summary, " Weapons: a random set each round.", sizeof( summary ), COPY_ALL_CHARACTERS );
	}
	Q_strncat( summary, "\n\nWhen the map loads, each player takes their screen with the right trigger (and picks a team in team games). The match starts when everyone is in.", sizeof( summary ), COPY_ALL_CHARACTERS );
	m_pSummary->SetText( summary );
}

//-----------------------------------------------------------------------------
// Messages
//-----------------------------------------------------------------------------
void CGESplitScreenMenu::OnTextChanged( vgui::Panel *panel )
{
	if ( m_bLoading )
		return;

	if ( panel == m_pProfileList )
	{
		// Picked another profile to edit
		StoreControlsIntoProfile();
		KeyValues *pData = m_pProfileList->GetActiveItemUserData();
		if ( pData )
		{
			m_iEditing = pData->GetInt( "index", 0 );
			LoadProfileIntoControls();
		}
		return;
	}

	if ( panel == m_pMap || panel == m_pMode || panel == m_pWeapons || panel == m_pWin || panel == m_pTime || panel == m_pKills || panel == m_pPerDisplay )
	{
		// The player picked a number: remember it, even when fewer controllers
		// are connected later
		// (the list reports its change later, so ignore the value RebuildPerDisplay set)
		if ( panel == m_pPerDisplay && !m_bLoading )
		{
			KeyValues *pData = m_pPerDisplay->GetActiveItemUserData();
			const int value = pData ? pData->GetInt( "value", m_iPerDisplayShown ) : m_iPerDisplayShown;
			if ( value != m_iPerDisplayShown )
			{
				m_iPerDisplayWanted = value;
				m_iPerDisplayShown = value;
			}
		}
		StoreControlsIntoMatch();
		UpdateMatchControls();
		return;
	}

	StoreControlsIntoProfile();
}

void CGESplitScreenMenu::OnSliderMoved( vgui::Panel *panel )
{
	UpdateSensitivityLabel();
	StoreControlsIntoProfile();
}

void CGESplitScreenMenu::OnCheckButtonChecked( vgui::Panel *panel )
{
	if ( m_bLoading )
		return;

	if ( panel == m_pTeams )
	{
		m_bTeamsChoice = m_pTeams->IsSelected();
		StoreControlsIntoMatch();
		UpdateMatchControls();
		return;
	}

	if ( panel == m_pJump || panel == m_pRadar )
	{
		StoreControlsIntoMatch();
		UpdateMatchControls();
		return;
	}

	StoreControlsIntoProfile();
}

void CGESplitScreenMenu::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "page0" ) )
	{
		SetPage( GESS_PAGE_PROFILES );
	}
	else if ( !Q_stricmp( command, "page1" ) )
	{
		SetPage( GESS_PAGE_MATCH );
	}
	else if ( !Q_stricmp( command, "new" ) )
	{
		StoreControlsIntoProfile();
		int idx = GESSProfiles()->Add();
		if ( idx >= 0 )
			RefreshProfileList( idx );
	}
	else if ( !Q_stricmp( command, "pickchar" ) )
	{
		if ( m_pCharButton->IsEnabled() )
			m_pCharPicker->Open( GetSelectedCharacter() );
	}
	else if ( !Q_stricmp( command, "delete" ) )
	{
		GESSProfiles()->Remove( m_iEditing );
		RefreshProfileList( m_iEditing - 1 );
	}
	else if ( !Q_stricmp( command, "recalibrate" ) )
	{
		GESSGamepads()->Recalibrate();
		m_flNextPadCalText = 0;
	}
	else if ( !Q_stricmp( command, "save" ) )
	{
		StoreControlsIntoProfile();
		GESSProfiles()->Save();
		RefreshProfileList( m_iEditing );
	}
	else if ( !Q_stricmp( command, "start" ) )
	{
		StoreControlsIntoProfile();
		GESSProfiles()->Save();
		StoreControlsIntoMatch();
		GESSMatch()->Start();
		Close();
	}
	else if ( !Q_stricmp( command, "close" ) )
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Commands
//-----------------------------------------------------------------------------
bool GESS_IsSplitScreenMenuOpen()
{
	return g_GESplitScreenMenu.GetNativePanel() && g_GESplitScreenMenu->IsVisible();
}

static void GESS_ToggleMenu( int page )
{
	if ( !g_GESplitScreenMenu.GetNativePanel() )
		return;

	if ( g_GESplitScreenMenu->IsVisible() && page < 0 )
		g_GESplitScreenMenu->Close();
	else
		g_GESplitScreenMenu->OpenPage( page < 0 ? GESS_PAGE_PROFILES : page );
}

CON_COMMAND( ges_splitscreen, "Open the Split Screen menu (profiles and match setup)" )
{
	GESS_ToggleMenu( -1 );
}

CON_COMMAND( ges_splitscreen_profiles, "Open the Split Screen menu on the Profiles page" )
{
	GESS_ToggleMenu( GESS_PAGE_PROFILES );
}

CON_COMMAND( ges_splitscreen_match, "Open the Split Screen menu on the Match page" )
{
	GESS_ToggleMenu( GESS_PAGE_MATCH );
}

CON_COMMAND( ges_splitscreen_pickchar, "Open the character picker for the selected split screen profile" )
{
	if ( !g_GESplitScreenMenu.GetNativePanel() )
		return;
	GESS_ToggleMenu( GESS_PAGE_PROFILES );
	g_GESplitScreenMenu->OnCommand( "pickchar" );
}

//-----------------------------------------------------------------------------
// Main menu hint until a controller has claimed player 1
//-----------------------------------------------------------------------------
class CGESSPadHint : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGESSPadHint, vgui::Panel );
public:
	CGESSPadHint( vgui::VPANEL parent ) : BaseClass( NULL, "GESSPadHint" )
	{
		SetParent( parent );
		SetScheme( vgui::scheme()->LoadSchemeFromFile( "resource/SourceScheme.res", "SourceScheme" ) );
		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );
		SetPaintBackgroundEnabled( false );
		SetVisible( false );
		m_hFont = INVALID_FONT;
		m_iFontScreenTall = 0;
		vgui::ivgui()->AddTickSignal( GetVPanel(), 100 );
	}

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		m_iFontScreenTall = 0;	// made again in OnTick, sized for the screen
		m_hFont = pScheme->GetFont( "DefaultLarge", false );
		if ( m_hFont == INVALID_FONT )
			m_hFont = pScheme->GetFont( "Default", false );
	}

	// Readable from the sofa: about 1/28 of the screen height
	void UpdateFont()
	{
		int sw, sh;
		vgui::surface()->GetScreenSize( sw, sh );
		if ( sh == m_iFontScreenTall )
			return;
		m_iFontScreenTall = sh;
		vgui::HFont font = vgui::surface()->CreateFont();
		if ( vgui::surface()->SetFontGlyphSet( font, "Verdana", Max( 14, sh / 28 ), 700, 0, 0, vgui::ISurface::FONTFLAG_ANTIALIAS | vgui::ISurface::FONTFLAG_DROPSHADOW ) )
			m_hFont = font;
	}

	virtual void OnTick()
	{
		UpdateFont();

		// Only on the main menu, not in a game or while a map loads
		const bool bMainMenu = enginevgui->IsGameUIVisible() && !engine->IsDrawingLoadingImage() &&
			( !engine->IsInGame() || engine->IsLevelMainMenuBackground() );
		const bool bShow = bMainMenu && GESplitScreenClient()->IsWaitingForHostPad();

		if ( bShow != IsVisible() )
		{
			SetVisible( bShow );
			if ( bShow && !vgui::input()->GetAppModalSurface() )
				MoveToFront();
		}

		if ( bShow )
		{
			int sw, sh;
			vgui::surface()->GetScreenSize( sw, sh );
			const int tall = m_hFont != INVALID_FONT ? vgui::surface()->GetFontTall( m_hFont ) : 20;
			SetBounds( 0, sh - tall * 3, sw, tall * 2 );
		}
	}

	virtual void Paint()
	{
		if ( m_hFont == INVALID_FONT )
			return;

		static const wchar_t *s_pszText = L"Press any button on a controller to be player 1";
		int tw, th;
		vgui::surface()->GetTextSize( m_hFont, s_pszText, tw, th );
		int w, h;
		GetSize( w, h );

		// Gently pulse so it's noticed
		const int alpha = 170 + (int)( 85.0f * sinf( (float) Plat_FloatTime() * 3.0f ) );
		vgui::surface()->DrawSetTextFont( m_hFont );
		vgui::surface()->DrawSetTextColor( Color( 0, 0, 0, alpha ) );
		vgui::surface()->DrawSetTextPos( ( w - tw ) / 2 + 2, ( h - th ) / 2 + 2 );
		vgui::surface()->DrawPrintText( s_pszText, wcslen( s_pszText ) );
		vgui::surface()->DrawSetTextColor( Color( 255, 255, 255, alpha ) );
		vgui::surface()->DrawSetTextPos( ( w - tw ) / 2, ( h - th ) / 2 );
		vgui::surface()->DrawPrintText( s_pszText, wcslen( s_pszText ) );
	}

private:
	vgui::HFont m_hFont;
	int m_iFontScreenTall;
};

static GameUI<CGESSPadHint> g_GESSPadHint;