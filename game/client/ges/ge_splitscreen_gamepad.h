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

	// ---- Automatic stick dead zones (ges_ss_pad_deadzone_auto) ----
	// Each stick's resting point and drift are measured while it's left alone, and
	// how far it reaches while it's pushed; saved per controller in
	// cfg/ges_splitscreen_pads.txt.
	struct StickCal_t
	{
		float	cx, cy;		// Resting point (subtracted)
		float	noise;		// How far it wanders from there at rest
		float	inner;		// Dead zone radius around the resting point
		float	reach;		// Furthest it has gone from the resting point (0 = not learned)
		bool	measured;	// Rest measured (else the fixed ges_ss_pad_deadzone is used)
	};
	const char *GetPadName( int index ) const;
	const char *GetPadId( int index ) const;
	const StickCal_t &GetStickCal( int index, int stick ) const;	// stick 0 left, 1 right
	float GetStickOuter( int index, int stick ) const;			// The reach used as 100%
	void Recalibrate();				// Forget the connected controllers' calibration, measure again
	// One controller, guided (R3 in the Split Screen lobby): rest, then turn the sticks round
	void RecalibratePad( int index );
	// What its card in the lobby says about calibrating (the R3 hint, or how it's going)
	const char *GetCalibrationPrompt( int index, char *buf, int bufLen ) const;
	bool IsCalibrating( int index ) const;
	void SaveCalibration();

private:
	struct PadCal_t
	{
		char		id[128];
		char		name[64];
		StickCal_t	stick[2];
	};
	// While a stick rests: samples of where it sits
	struct RestWindow_t
	{
		int		count;
		double	start;
		float	sumX, sumY, minX, maxX, minY, maxY, lastX, lastY;
		bool	sessionMeasured;	// Measured since it was plugged in (later ones only widen)
	};
	void LoadCalibration();
	int  FindCal( const char *id, const char *name, bool bCreate );
	void UpdateCalibration( int index, int stick, float x, float y, bool bIdle );
	void ApplyStick( int index, int stick, float &x, float &y ) const;

	IGESSPadBackend *m_pBackend;
	GESSPadState_t m_Pads[GES_SS_MAX_PADS];
	char		m_szPadId[GES_SS_MAX_PADS][128];
	char		m_szPadName[GES_SS_MAX_PADS][64];
	int			m_iPadCal[GES_SS_MAX_PADS];		// Index in m_Cals, -1 = none
	RestWindow_t m_Rest[GES_SS_MAX_PADS][2];
	int			m_iGuidedCal[GES_SS_MAX_PADS];	// RecalibratePad: 0 none, 1 resting, 2 turning, 3 done
	double		m_flGuidedCalStart[GES_SS_MAX_PADS];
	void UpdateGuidedCalibration( int index );
	CUtlVector<PadCal_t> m_Cals;
	bool		m_bCalLoaded;
	bool		m_bCalDirty;
	double		m_flNextCalSave;
};

CGESSGamepads *GESSGamepads();

#endif // GE_SPLITSCREEN_GAMEPAD_H
