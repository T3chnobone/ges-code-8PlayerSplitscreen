///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_splitscreen_padreader.h
//   Description :
//      Turns controller input into menu presses (Split Screen menu, custom
//      character picker).
//
////////////////////////////////////////////////////////////////////////////
#ifndef GE_SPLITSCREEN_PADREADER_H
#define GE_SPLITSCREEN_PADREADER_H
#ifdef _WIN32
#pragma once
#endif

#include "ge_splitscreen_client.h"
#include "ge_splitscreen_gamepad.h"

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

#endif // GE_SPLITSCREEN_PADREADER_H
