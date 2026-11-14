///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_splitscreen_gamepad.h
//   Description :
//      Multi-controller input for split screen.
//
//      The Source 2013 input system only exposes one joystick (MAX_JOYSTICKS
//      is 1 in the engine), so split screen reads controllers itself:
//        * SDL2's GameController API when SDL2 can be loaded (all platforms,
//          up to 8 controllers). The engine ships SDL2 on Linux/macOS; on
//          Windows SDL2.dll must be placed in the game or mod bin folder.
//        * XInput on Windows as a fallback (up to 4 controllers).
//
////////////////////////////////////////////////////////////////////////////

#ifndef GE_SPLITSCREEN_GAMEPAD_H
#define GE_SPLITSCREEN_GAMEPAD_H
#ifdef _WIN32
#pragma once
#endif

#define GES_SS_MAX_PADS 8

enum GESSPadButton_t
{
	GESS_PAD_A = 0,
	GESS_PAD_B,
	GESS_PAD_X,
	GESS_PAD_Y,
	GESS_PAD_BACK,
	GESS_PAD_GUIDE,
	GESS_PAD_START,
	GESS_PAD_LSTICK,
	GESS_PAD_RSTICK,
	GESS_PAD_LB,
	GESS_PAD_RB,
	GESS_PAD_DPAD_UP,
	GESS_PAD_DPAD_DOWN,
	GESS_PAD_DPAD_LEFT,
	GESS_PAD_DPAD_RIGHT,
	GESS_PAD_LT,		// Trigger treated as a button past the threshold
	GESS_PAD_RT,		// Trigger treated as a button past the threshold

	GESS_PAD_BUTTON_COUNT
};

#define GESS_PADBIT( b ) ( 1u << (b) )

struct GESSPadState_t
{
	bool	connected;
	// Sticks in [-1, 1] after dead zone. Y is positive when pushed UP.
	float	lx, ly, rx, ry;
	// Triggers in [0, 1]
	float	lt, rt;
	// Button bits (GESS_PADBIT)
	unsigned int buttons;
	unsigned int pressed;	// went down this poll
	unsigned int released;	// went up this poll

	bool IsDown( GESSPadButton_t b ) const		{ return ( buttons & GESS_PADBIT( b ) ) != 0; }
	bool WasPressed( GESSPadButton_t b ) const	{ return ( pressed & GESS_PADBIT( b ) ) != 0; }
	bool WasReleased( GESSPadButton_t b ) const	{ return ( released & GESS_PADBIT( b ) ) != 0; }
};

class IGESSPadBackend;

class CGESSGamepads
{
public:
	CGESSGamepads();
	~CGESSGamepads();

	bool Init();
	void Shutdown();

	// Read every controller. Call once per frame.
	void Poll();

	// Controller state for pad index [0, GES_SS_MAX_PADS). Pad indexes are stable while
	// a controller stays plugged in, and a re-plugged controller takes the first free index.
	const GESSPadState_t &GetPad( int index ) const;
	int  GetConnectedCount() const;
	int  GetMaxPads() const;
	const char *GetBackendName() const;

	// Clear pressed/released edges (used when a menu consumes them)
	void ConsumeEdges( int index );

private:
	IGESSPadBackend *m_pBackend;
	GESSPadState_t m_Pads[GES_SS_MAX_PADS];
};

CGESSGamepads *GESSGamepads();

#endif // GE_SPLITSCREEN_GAMEPAD_H
