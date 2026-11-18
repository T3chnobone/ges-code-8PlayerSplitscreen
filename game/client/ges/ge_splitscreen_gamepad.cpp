///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_splitscreen_gamepad.cpp
//   Description :
//      Multi-controller input for split screen. See ge_splitscreen_gamepad.h
//
////////////////////////////////////////////////////////////////////////////

#include "cbase.h"

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
	#include <dlfcn.h>
#endif

#include "ge_splitscreen_gamepad.h"
#include "filesystem.h"
#include "ienginevgui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar ges_ss_pad_deadzone( "ges_ss_pad_deadzone", "0.22", FCVAR_ARCHIVE, "Split screen controller stick dead zone (0-0.9).", true, 0.0f, true, 0.9f );
ConVar ges_ss_pad_trigger_threshold( "ges_ss_pad_trigger_threshold", "0.4", FCVAR_ARCHIVE, "How far a trigger must be pulled to count as pressed.", true, 0.05f, true, 0.95f );
ConVar ges_ss_pad_backend( "ges_ss_pad_backend", "auto", FCVAR_ARCHIVE, "Split screen controller backend: auto, sdl or xinput (Windows). Takes effect on restart." );

//-----------------------------------------------------------------------------
// Dynamic library helpers (no link time dependency on SDL2 / XInput)
//-----------------------------------------------------------------------------
static void *GESS_LoadLibrary( const char *name )
{
#ifdef _WIN32
	return (void*) LoadLibraryA( name );
#else
	return dlopen( name, RTLD_NOW | RTLD_LOCAL );
#endif
}

static void *GESS_GetSymbol( void *lib, const char *name )
{
	if ( !lib )
		return NULL;
#ifdef _WIN32
	return (void*) GetProcAddress( (HMODULE) lib, name );
#else
	return dlsym( lib, name );
#endif
}

static void GESS_FreeLibrary( void *lib )
{
	if ( !lib )
		return;
#ifdef _WIN32
	FreeLibrary( (HMODULE) lib );
#else
	dlclose( lib );
#endif
}

//-----------------------------------------------------------------------------
// Raw (pre dead zone) state a backend produces
//-----------------------------------------------------------------------------
struct GESSRawPad_t
{
	bool connected;
	float lx, ly, rx, ry, lt, rt;	// sticks [-1,1] (y up positive), triggers [0,1]
	unsigned int buttons;			// GESS_PADBIT excluding triggers
	// Which controller this is, the same every time it's plugged in (its saved
	// stick calibration is found by it), and a name to show. Owned by the backend.
	const char *id;
	const char *name;
};

class IGESSPadBackend
{
public:
	virtual ~IGESSPadBackend() {}
	virtual bool Init() = 0;
	virtual void Shutdown() = 0;
	virtual void Poll( GESSRawPad_t *pads, int maxPads ) = 0;
	virtual int  MaxPads() const = 0;
	virtual const char *Name() const = 0;
};

//=============================================================================
// SDL2 GameController backend
//=============================================================================
namespace
{
	// Minimal SDL2 ABI, matches SDL 2.0.x
	typedef struct _SDL_GameController SDL_GameController;
	typedef struct _SDL_Joystick SDL_Joystick;
	typedef int SDL_JoystickID;

	enum
	{
		GESS_SDL_INIT_JOYSTICK = 0x00000200u,
		GESS_SDL_INIT_GAMECONTROLLER = 0x00002000u,
	};

	enum
	{
		GESS_SDL_AXIS_LEFTX = 0, GESS_SDL_AXIS_LEFTY, GESS_SDL_AXIS_RIGHTX, GESS_SDL_AXIS_RIGHTY,
		GESS_SDL_AXIS_TRIGGERLEFT, GESS_SDL_AXIS_TRIGGERRIGHT,
	};

	// SDL_GameControllerButton order matches GESSPadButton_t for A..DPAD_RIGHT
	enum { GESS_SDL_BUTTON_COUNT = 15 };

	typedef unsigned int	( *SDL_WasInit_t )( unsigned int );
	typedef int				( *SDL_SetHint_t )( const char *, const char * );
	typedef int				( *SDL_InitSubSystem_t )( unsigned int );
	typedef void			( *SDL_QuitSubSystem_t )( unsigned int );
	typedef int				( *SDL_NumJoysticks_t )( void );
	typedef int				( *SDL_IsGameController_t )( int );
	typedef SDL_GameController* ( *SDL_GameControllerOpen_t )( int );
	typedef void			( *SDL_GameControllerClose_t )( SDL_GameController* );
	typedef int				( *SDL_GameControllerGetAttached_t )( SDL_GameController* );
	typedef short			( *SDL_GameControllerGetAxis_t )( SDL_GameController*, int );
	typedef unsigned char	( *SDL_GameControllerGetButton_t )( SDL_GameController*, int );
	typedef void			( *SDL_GameControllerUpdate_t )( void );
	typedef SDL_Joystick*	( *SDL_GameControllerGetJoystick_t )( SDL_GameController* );
	typedef SDL_JoystickID	( *SDL_JoystickInstanceID_t )( SDL_Joystick* );
	typedef SDL_JoystickID	( *SDL_JoystickGetDeviceInstanceID_t )( int );
	// Identity (optional: SDL 2.0.x has the GUID and name, 2.0.14+ the serial)
	struct GESS_SDL_JoystickGUID { unsigned char data[16]; };
	typedef GESS_SDL_JoystickGUID ( *SDL_JoystickGetGUID_t )( SDL_Joystick* );
	typedef void			( *SDL_JoystickGetGUIDString_t )( GESS_SDL_JoystickGUID, char*, int );
	typedef const char*		( *SDL_GameControllerGetSerial_t )( SDL_GameController* );
	typedef const char*		( *SDL_GameControllerName_t )( SDL_GameController* );
}

class CGESSPadBackendSDL : public IGESSPadBackend
{
public:
	CGESSPadBackendSDL() : m_pLib( NULL ), m_bInitedSubsystem( false ), m_flNextScan( 0 )
	{
		Q_strncpy( m_szName, "SDL2", sizeof( m_szName ) );
		for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
		{
			m_pControllers[i] = NULL;
			m_szId[i][0] = 0;
			m_szPadName[i][0] = 0;
		}
		m_SDL_JoystickGetGUID = NULL;
		m_SDL_JoystickGetGUIDString = NULL;
		m_SDL_GameControllerGetSerial = NULL;
		m_SDL_GameControllerName = NULL;
	}

	virtual bool Init()
	{
		static const char *s_pszNames[] =
		{
#ifdef _WIN32
			"SDL2.dll",
#elif defined( OSX )
			"libSDL2-2.0.0.dylib",
			"libSDL2.dylib",
#else
			"libSDL2-2.0.so.0",
			"libSDL2.so",
#endif
		};

		// A newer SDL2.dll in the mod's bin folder wins (better controller support),
		// then the normal search path, then the engine's own copy next to engine.dll
#ifdef _WIN32
		{
			char szPath[MAX_PATH];
			Q_snprintf( szPath, sizeof( szPath ), "%s\\bin\\SDL2.dll", engine->GetGameDirectory() );
			m_pLib = GESS_LoadLibrary( szPath );
		}
#endif

		for ( int i = 0; i < ARRAYSIZE( s_pszNames ) && !m_pLib; i++ )
			m_pLib = GESS_LoadLibrary( s_pszNames[i] );

#ifdef _WIN32
		if ( !m_pLib )
		{
			HMODULE hEngine = GetModuleHandleA( "engine.dll" );
			char szPath[MAX_PATH];
			if ( hEngine && GetModuleFileNameA( hEngine, szPath, sizeof( szPath ) ) )
			{
				char *pSlash = Q_strrchr( szPath, '\\' );
				if ( pSlash )
				{
					Q_strncpy( pSlash + 1, "SDL2.dll", sizeof( szPath ) - ( pSlash + 1 - szPath ) );
					m_pLib = GESS_LoadLibrary( szPath );
				}
			}
		}
#endif

		if ( !m_pLib )
			return false;

#define GESS_SDL_SYM( name ) m_##name = (name##_t) GESS_GetSymbol( m_pLib, #name ); if ( !m_##name ) { Shutdown(); return false; }
		GESS_SDL_SYM( SDL_WasInit );
		GESS_SDL_SYM( SDL_InitSubSystem );
		GESS_SDL_SYM( SDL_QuitSubSystem );
		GESS_SDL_SYM( SDL_NumJoysticks );
		GESS_SDL_SYM( SDL_IsGameController );
		GESS_SDL_SYM( SDL_GameControllerOpen );
		GESS_SDL_SYM( SDL_GameControllerClose );
		GESS_SDL_SYM( SDL_GameControllerGetAttached );
		GESS_SDL_SYM( SDL_GameControllerGetAxis );
		GESS_SDL_SYM( SDL_GameControllerGetButton );
		GESS_SDL_SYM( SDL_GameControllerUpdate );
		GESS_SDL_SYM( SDL_GameControllerGetJoystick );
		GESS_SDL_SYM( SDL_JoystickInstanceID );
		// SDL 2.0.6+ only (the engine ships 2.0.4): optional
		m_SDL_JoystickGetDeviceInstanceID = (SDL_JoystickGetDeviceInstanceID_t) GESS_GetSymbol( m_pLib, "SDL_JoystickGetDeviceInstanceID" );
		m_SDL_JoystickGetGUID = (SDL_JoystickGetGUID_t) GESS_GetSymbol( m_pLib, "SDL_JoystickGetGUID" );
		m_SDL_JoystickGetGUIDString = (SDL_JoystickGetGUIDString_t) GESS_GetSymbol( m_pLib, "SDL_JoystickGetGUIDString" );
		m_SDL_GameControllerGetSerial = (SDL_GameControllerGetSerial_t) GESS_GetSymbol( m_pLib, "SDL_GameControllerGetSerial" );
		m_SDL_GameControllerName = (SDL_GameControllerName_t) GESS_GetSymbol( m_pLib, "SDL_GameControllerName" );
#undef GESS_SDL_SYM

		// We read the controllers ourselves, focus or not (SDL doesn't own our window)
		SDL_SetHint_t pSetHint = (SDL_SetHint_t) GESS_GetSymbol( m_pLib, "SDL_SetHint" );
		if ( pSetHint )
		{
			pSetHint( "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1" );
			// Xbox pads through XInput. RawInput and Windows.Gaming.Input need SDL to
			// own the window message loop, which it doesn't here, so they'd see nothing.
			// HIDAPI drives PlayStation / Switch pads directly. (Older SDL ignores these.)
			pSetHint( "SDL_XINPUT_ENABLED", "1" );
			pSetHint( "SDL_JOYSTICK_RAWINPUT", "0" );
			pSetHint( "SDL_JOYSTICK_WGI", "0" );
			pSetHint( "SDL_JOYSTICK_HIDAPI", "1" );
			// ...but not for Xbox pads, which XInput already gives us (they'd show up twice)
			pSetHint( "SDL_JOYSTICK_HIDAPI_XBOX", "0" );
			pSetHint( "SDL_JOYSTICK_HIDAPI_XBOX_360", "0" );
			pSetHint( "SDL_JOYSTICK_HIDAPI_XBOX_ONE", "0" );
		}

		if ( !( m_SDL_WasInit( GESS_SDL_INIT_GAMECONTROLLER ) & GESS_SDL_INIT_GAMECONTROLLER ) )
		{
			if ( m_SDL_InitSubSystem( GESS_SDL_INIT_JOYSTICK | GESS_SDL_INIT_GAMECONTROLLER ) != 0 )
			{
				Shutdown();
				return false;
			}
			m_bInitedSubsystem = true;
		}

		// Which SDL we got, for ges_ss_status
		Q_strncpy( m_szName, "SDL2", sizeof( m_szName ) );
		typedef void ( *SDL_GetVersion_t )( unsigned char * );
		SDL_GetVersion_t pGetVersion = (SDL_GetVersion_t) GESS_GetSymbol( m_pLib, "SDL_GetVersion" );
		if ( pGetVersion )
		{
			unsigned char ver[3] = { 0, 0, 0 };	// SDL_version: major, minor, patch
			pGetVersion( ver );
			Q_snprintf( m_szName, sizeof( m_szName ), "SDL %d.%d.%d", ver[0], ver[1], ver[2] );
		}

		Scan();
		return true;
	}

	virtual void Shutdown()
	{
		if ( m_pLib && m_SDL_GameControllerClose )
		{
			for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
			{
				if ( m_pControllers[i] )
					m_SDL_GameControllerClose( m_pControllers[i] );
				m_pControllers[i] = NULL;
				m_szId[i][0] = 0;
			}
		}

		if ( m_bInitedSubsystem && m_SDL_QuitSubSystem )
			m_SDL_QuitSubSystem( GESS_SDL_INIT_JOYSTICK | GESS_SDL_INIT_GAMECONTROLLER );
		m_bInitedSubsystem = false;

		GESS_FreeLibrary( m_pLib );
		m_pLib = NULL;
	}

	virtual void Poll( GESSRawPad_t *pads, int maxPads )
	{
		m_SDL_GameControllerUpdate();

		// Look for new controllers now and then (cheap, but no need every frame)
		if ( gpGlobals->realtime >= m_flNextScan )
			Scan();

		for ( int i = 0; i < maxPads && i < GES_SS_MAX_PADS; i++ )
		{
			GESSRawPad_t &p = pads[i];
			memset( &p, 0, sizeof( p ) );

			SDL_GameController *c = m_pControllers[i];
			if ( !c )
				continue;

			if ( !m_SDL_GameControllerGetAttached( c ) )
			{
				m_SDL_GameControllerClose( c );
				m_pControllers[i] = NULL;
				m_szId[i][0] = 0;
				continue;
			}

			p.connected = true;
			if ( !m_szId[i][0] )
				FillIdentity( i );
			p.id = m_szId[i];
			p.name = m_szPadName[i];
			p.lx = AxisToFloat( m_SDL_GameControllerGetAxis( c, GESS_SDL_AXIS_LEFTX ) );
			p.ly = -AxisToFloat( m_SDL_GameControllerGetAxis( c, GESS_SDL_AXIS_LEFTY ) );
			p.rx = AxisToFloat( m_SDL_GameControllerGetAxis( c, GESS_SDL_AXIS_RIGHTX ) );
			p.ry = -AxisToFloat( m_SDL_GameControllerGetAxis( c, GESS_SDL_AXIS_RIGHTY ) );
			p.lt = MAX( 0.0f, AxisToFloat( m_SDL_GameControllerGetAxis( c, GESS_SDL_AXIS_TRIGGERLEFT ) ) );
			p.rt = MAX( 0.0f, AxisToFloat( m_SDL_GameControllerGetAxis( c, GESS_SDL_AXIS_TRIGGERRIGHT ) ) );

			for ( int b = 0; b < GESS_SDL_BUTTON_COUNT; b++ )
			{
				if ( m_SDL_GameControllerGetButton( c, b ) )
					p.buttons |= GESS_PADBIT( b );
			}
		}
	}

	virtual int MaxPads() const { return GES_SS_MAX_PADS; }
	virtual const char *Name() const { return m_szName; }

private:
	static float AxisToFloat( short v ) { return v < 0 ? v / 32768.0f : v / 32767.0f; }

	// The controller's identity: its SDL GUID (make, model, connection) plus its
	// serial number when SDL can read one, else plus its name. Two identical
	// controllers without serials get the same identity.
	void FillIdentity( int i )
	{
		SDL_GameController *c = m_pControllers[i];
		const char *pszName = m_SDL_GameControllerName ? m_SDL_GameControllerName( c ) : NULL;
		Q_strncpy( m_szPadName[i], pszName && pszName[0] ? pszName : "Controller", sizeof( m_szPadName[i] ) );

		char szGUID[64] = "noguid";
		if ( m_SDL_JoystickGetGUID && m_SDL_JoystickGetGUIDString )
			m_SDL_JoystickGetGUIDString( m_SDL_JoystickGetGUID( m_SDL_GameControllerGetJoystick( c ) ), szGUID, sizeof( szGUID ) );

		const char *pszSerial = m_SDL_GameControllerGetSerial ? m_SDL_GameControllerGetSerial( c ) : NULL;
		if ( pszSerial && pszSerial[0] )
			Q_snprintf( m_szId[i], sizeof( m_szId[i] ), "%s/serial:%s", szGUID, pszSerial );
		else
			Q_snprintf( m_szId[i], sizeof( m_szId[i] ), "%s/%s", szGUID, m_szPadName[i] );
	}

	char m_szId[GES_SS_MAX_PADS][128];
	char m_szPadName[GES_SS_MAX_PADS][64];
	SDL_JoystickGetGUID_t m_SDL_JoystickGetGUID;
	SDL_JoystickGetGUIDString_t m_SDL_JoystickGetGUIDString;
	SDL_GameControllerGetSerial_t m_SDL_GameControllerGetSerial;
	SDL_GameControllerName_t m_SDL_GameControllerName;

	void Scan()
	{
		m_flNextScan = gpGlobals->realtime + 1.0f;

		const int count = m_SDL_NumJoysticks();
		for ( int dev = 0; dev < count; dev++ )
		{
			if ( !m_SDL_IsGameController( dev ) )
				continue;

			int freeSlot = -1;
			for ( int i = 0; i < GES_SS_MAX_PADS && freeSlot < 0; i++ )
			{
				if ( !m_pControllers[i] )
					freeSlot = i;
			}

			if ( m_SDL_JoystickGetDeviceInstanceID )
			{
				// Already open?
				SDL_JoystickID id = m_SDL_JoystickGetDeviceInstanceID( dev );
				bool bOpen = false;
				for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
				{
					if ( m_pControllers[i] && m_SDL_JoystickInstanceID( m_SDL_GameControllerGetJoystick( m_pControllers[i] ) ) == id )
						bOpen = true;
				}

				if ( bOpen || freeSlot < 0 )
					continue;

				m_pControllers[freeSlot] = m_SDL_GameControllerOpen( dev );
			}
			else
			{
				// Older SDL: opening a controller that's already open returns the same
				// handle (with one more reference), which tells us we have it already
				SDL_GameController *c = m_SDL_GameControllerOpen( dev );
				if ( !c )
					continue;

				bool bOpen = false;
				for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
				{
					if ( m_pControllers[i] == c )
						bOpen = true;
				}

				if ( bOpen || freeSlot < 0 )
					m_SDL_GameControllerClose( c );	// drop the extra reference
				else
					m_pControllers[freeSlot] = c;
			}
		}
	}

	void *m_pLib;
	bool m_bInitedSubsystem;
	char m_szName[32];
	float m_flNextScan;
	SDL_GameController *m_pControllers[GES_SS_MAX_PADS];

	SDL_WasInit_t m_SDL_WasInit;
	SDL_InitSubSystem_t m_SDL_InitSubSystem;
	SDL_QuitSubSystem_t m_SDL_QuitSubSystem;
	SDL_NumJoysticks_t m_SDL_NumJoysticks;
	SDL_IsGameController_t m_SDL_IsGameController;
	SDL_GameControllerOpen_t m_SDL_GameControllerOpen;
	SDL_GameControllerClose_t m_SDL_GameControllerClose;
	SDL_GameControllerGetAttached_t m_SDL_GameControllerGetAttached;
	SDL_GameControllerGetAxis_t m_SDL_GameControllerGetAxis;
	SDL_GameControllerGetButton_t m_SDL_GameControllerGetButton;
	SDL_GameControllerUpdate_t m_SDL_GameControllerUpdate;
	SDL_GameControllerGetJoystick_t m_SDL_GameControllerGetJoystick;
	SDL_JoystickInstanceID_t m_SDL_JoystickInstanceID;
	SDL_JoystickGetDeviceInstanceID_t m_SDL_JoystickGetDeviceInstanceID;
};

//=============================================================================
// XInput backend (Windows only, 4 controllers)
//=============================================================================
#ifdef _WIN32
namespace
{
	struct GESS_XINPUT_GAMEPAD
	{
		WORD  wButtons;
		BYTE  bLeftTrigger;
		BYTE  bRightTrigger;
		SHORT sThumbLX;
		SHORT sThumbLY;
		SHORT sThumbRX;
		SHORT sThumbRY;
	};

	struct GESS_XINPUT_STATE
	{
		DWORD dwPacketNumber;
		GESS_XINPUT_GAMEPAD Gamepad;
	};

	typedef DWORD ( WINAPI *XInputGetState_t )( DWORD, GESS_XINPUT_STATE* );

	// XInput button bits -> our buttons
	struct XInputMap_t { WORD xbit; int button; };
	const XInputMap_t s_XInputMap[] =
	{
		{ 0x1000, GESS_PAD_A }, { 0x2000, GESS_PAD_B }, { 0x4000, GESS_PAD_X }, { 0x8000, GESS_PAD_Y },
		{ 0x0020, GESS_PAD_BACK }, { 0x0010, GESS_PAD_START },
		{ 0x0040, GESS_PAD_LSTICK }, { 0x0080, GESS_PAD_RSTICK },
		{ 0x0100, GESS_PAD_LB }, { 0x0200, GESS_PAD_RB },
		{ 0x0001, GESS_PAD_DPAD_UP }, { 0x0002, GESS_PAD_DPAD_DOWN },
		{ 0x0004, GESS_PAD_DPAD_LEFT }, { 0x0008, GESS_PAD_DPAD_RIGHT },
	};
}

class CGESSPadBackendXInput : public IGESSPadBackend
{
public:
	CGESSPadBackendXInput() : m_pLib( NULL ), m_GetState( NULL ) {}

	virtual bool Init()
	{
		static const char *s_pszNames[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
		for ( int i = 0; i < ARRAYSIZE( s_pszNames ) && !m_pLib; i++ )
			m_pLib = GESS_LoadLibrary( s_pszNames[i] );

		m_GetState = (XInputGetState_t) GESS_GetSymbol( m_pLib, "XInputGetState" );
		if ( !m_GetState )
		{
			Shutdown();
			return false;
		}
		for ( int i = 0; i < 4; i++ )
			m_flNextRetry[i] = 0;
		return true;
	}

	virtual void Shutdown()
	{
		GESS_FreeLibrary( m_pLib );
		m_pLib = NULL;
		m_GetState = NULL;
	}

	virtual void Poll( GESSRawPad_t *pads, int maxPads )
	{
		for ( int i = 0; i < maxPads; i++ )
		{
			GESSRawPad_t &p = pads[i];
			memset( &p, 0, sizeof( p ) );
			if ( i >= 4 )
				continue;

			// Polling a disconnected XInput pad is slow, only retry every second
			if ( gpGlobals->realtime < m_flNextRetry[i] )
				continue;

			GESS_XINPUT_STATE st;
			memset( &st, 0, sizeof( st ) );
			if ( m_GetState( i, &st ) != 0 )
			{
				m_flNextRetry[i] = gpGlobals->realtime + 1.0f;
				continue;
			}

			p.connected = true;
			// XInput can't tell controllers apart: one identity per port
			static const char *s_pszIds[4] = { "xinput/1", "xinput/2", "xinput/3", "xinput/4" };
			p.id = s_pszIds[i];
			p.name = "XInput controller";
			p.lx = st.Gamepad.sThumbLX / 32767.0f;
			p.ly = st.Gamepad.sThumbLY / 32767.0f;
			p.rx = st.Gamepad.sThumbRX / 32767.0f;
			p.ry = st.Gamepad.sThumbRY / 32767.0f;
			p.lt = st.Gamepad.bLeftTrigger / 255.0f;
			p.rt = st.Gamepad.bRightTrigger / 255.0f;

			for ( int b = 0; b < ARRAYSIZE( s_XInputMap ); b++ )
			{
				if ( st.Gamepad.wButtons & s_XInputMap[b].xbit )
					p.buttons |= GESS_PADBIT( s_XInputMap[b].button );
			}
		}
	}

	virtual int MaxPads() const { return 4; }
	virtual const char *Name() const { return "XInput"; }

private:
	void *m_pLib;
	XInputGetState_t m_GetState;
	float m_flNextRetry[4];
};
#endif // _WIN32

//=============================================================================
// CGESSGamepads
//=============================================================================
static CGESSGamepads g_GESSGamepads;
CGESSGamepads *GESSGamepads() { return &g_GESSGamepads; }

static GESSPadState_t s_EmptyPad;

CGESSGamepads::CGESSGamepads() : m_pBackend( NULL )
{
	memset( m_Pads, 0, sizeof( m_Pads ) );
	memset( &s_EmptyPad, 0, sizeof( s_EmptyPad ) );
	memset( m_Rest, 0, sizeof( m_Rest ) );
	memset( m_RawStick, 0, sizeof( m_RawStick ) );
	memset( m_bPadInMenu, 0, sizeof( m_bPadInMenu ) );
	for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
	{
		m_szPadId[i][0] = 0;
		m_szPadName[i][0] = 0;
		m_iPadCal[i] = -1;
		m_iGuidedCal[i] = 0;
		m_flGuidedCalStart[i] = 0;
	}
	m_bCalLoaded = false;
	m_bCalDirty = false;
	m_flNextCalSave = 0;
}

CGESSGamepads::~CGESSGamepads()
{
	// Shutdown() is expected to have been called
}

bool CGESSGamepads::Init()
{
	if ( m_pBackend )
		return true;

	const char *pszBackend = ges_ss_pad_backend.GetString();
	const bool bAuto = !Q_stricmp( pszBackend, "auto" );

	if ( bAuto || !Q_stricmp( pszBackend, "sdl" ) )
	{
		m_pBackend = new CGESSPadBackendSDL();
		if ( !m_pBackend->Init() )
		{
			delete m_pBackend;
			m_pBackend = NULL;
		}
	}

#ifdef _WIN32
	if ( !m_pBackend && ( bAuto || !Q_stricmp( pszBackend, "xinput" ) ) )
	{
		m_pBackend = new CGESSPadBackendXInput();
		if ( !m_pBackend->Init() )
		{
			delete m_pBackend;
			m_pBackend = NULL;
		}
	}
#endif

	if ( m_pBackend )
		DevMsg( "Split screen: using %s for controllers (up to %d)\n", m_pBackend->Name(), m_pBackend->MaxPads() );
	else
		Warning( "Split screen: no controller backend available (SDL2 or XInput), local players can't join.\n" );

	return m_pBackend != NULL;
}

void CGESSGamepads::Shutdown()
{
	if ( m_bCalDirty )
		SaveCalibration();

	if ( m_pBackend )
	{
		m_pBackend->Shutdown();
		delete m_pBackend;
		m_pBackend = NULL;
	}
	memset( m_Pads, 0, sizeof( m_Pads ) );
}

static void ApplyRadialDeadzone( float &x, float &y, float deadzone )
{
	float len = sqrtf( x * x + y * y );
	if ( len <= deadzone )
	{
		x = y = 0.0f;
		return;
	}

	// Rescale so the edge of the dead zone maps to 0 and clamp to the unit circle
	float scaled = ( MIN( len, 1.0f ) - deadzone ) / ( 1.0f - deadzone );
	x = ( x / len ) * scaled;
	y = ( y / len ) * scaled;
}

// Testing: override a controller's right stick / right trigger for a while
struct GESSDebugPad_t { float rx, ry, rt, until; unsigned int buttons; double buttonsUntil; };
static GESSDebugPad_t s_DebugPads[GES_SS_MAX_PADS];

CON_COMMAND_F( ges_ss_debug_padbutton, "Testing: ges_ss_debug_padbutton <controller 1-8> <button: 0 A, 1 B, 6 START, 8 R3...> [seconds] holds a controller button", FCVAR_DONTRECORD )
{
	if ( args.ArgC() < 3 )
		return;
	const int pad = atoi( args[1] ) - 1, button = atoi( args[2] );
	if ( pad < 0 || pad >= GES_SS_MAX_PADS || button < 0 || button >= GESS_PAD_BUTTON_COUNT )
		return;
	s_DebugPads[pad].buttons = GESS_PADBIT( button );
	s_DebugPads[pad].buttonsUntil = Plat_FloatTime() + ( args.ArgC() > 3 ? atof( args[3] ) : 0.2 );
}

CON_COMMAND_F( ges_ss_debug_pad, "Testing: ges_ss_debug_pad <controller 1-8> <right x> <right y> [right trigger] [seconds]", FCVAR_DONTRECORD )
{
	if ( args.ArgC() < 4 )
		return;
	const int pad = atoi( args[1] ) - 1;
	if ( pad < 0 || pad >= GES_SS_MAX_PADS )
		return;
	GESSDebugPad_t &d = s_DebugPads[pad];
	d.rx = clamp( (float) atof( args[2] ), -1.0f, 1.0f );
	d.ry = clamp( (float) atof( args[3] ), -1.0f, 1.0f );
	d.rt = args.ArgC() > 4 ? clamp( (float) atof( args[4] ), 0.0f, 1.0f ) : 0.0f;
	d.until = Plat_FloatTime() + ( args.ArgC() > 5 ? atof( args[5] ) : 1.0 );
	Msg( "ges_ss_debug_pad: controller %d right stick %.2f %.2f trigger %.2f until %.1f\n", pad + 1, d.rx, d.ry, d.rt, d.until );
}

void CGESSGamepads::Poll()
{
	GESSRawPad_t raw[GES_SS_MAX_PADS];
	memset( raw, 0, sizeof( raw ) );

	if ( m_pBackend )
		m_pBackend->Poll( raw, GES_SS_MAX_PADS );

	const float now = Plat_FloatTime();
	for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
	{
		const GESSDebugPad_t &d = s_DebugPads[i];
		if ( raw[i].connected && now < d.until )
		{
			raw[i].rx = d.rx;
			raw[i].ry = d.ry;
			raw[i].rt = d.rt;
		}
		if ( raw[i].connected && now < d.buttonsUntil )
			raw[i].buttons |= d.buttons;
	}

	const float trigger = ges_ss_pad_trigger_threshold.GetFloat();

	if ( !m_bCalLoaded )
		LoadCalibration();

	for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
	{
		GESSPadState_t &p = m_Pads[i];
		const unsigned int oldButtons = p.buttons;

		// Which controller: its saved calibration, and a fresh measurement session
		const char *pszId = raw[i].connected && raw[i].id ? raw[i].id : "";
		if ( Q_strcmp( pszId, m_szPadId[i] ) )
		{
			Q_strncpy( m_szPadId[i], pszId, sizeof( m_szPadId[i] ) );
			Q_strncpy( m_szPadName[i], raw[i].connected && raw[i].name ? raw[i].name : "", sizeof( m_szPadName[i] ) );
			m_iPadCal[i] = pszId[0] ? FindCal( pszId, m_szPadName[i], true ) : -1;
			memset( m_Rest[i], 0, sizeof( m_Rest[i] ) );
			m_iGuidedCal[i] = 0;
		}

		p.connected = raw[i].connected;
		p.lx = raw[i].lx; p.ly = raw[i].ly;
		p.rx = raw[i].rx; p.ry = raw[i].ry;
		m_RawStick[i][0] = p.lx; m_RawStick[i][1] = p.ly;
		m_RawStick[i][2] = p.rx; m_RawStick[i][3] = p.ry;
		if ( p.connected )
		{
			// Left alone: nothing pressed, triggers up, and not in the middle of a game
			// (where a stick held gently and steadily would look like it's resting),
			// unless this controller's player has a menu open
			const bool bInMenus = !engine->IsInGame() || enginevgui->IsGameUIVisible() || m_bPadInMenu[i];
			const bool bIdle = bInMenus && raw[i].buttons == 0 && raw[i].lt < 0.1f && raw[i].rt < 0.1f;
			UpdateCalibration( i, 0, p.lx, p.ly, bIdle );
			UpdateCalibration( i, 1, p.rx, p.ry, bIdle );
			UpdateGuidedCalibration( i );
		}
		ApplyStick( i, 0, p.lx, p.ly );
		ApplyStick( i, 1, p.rx, p.ry );
		p.lt = raw[i].lt;
		p.rt = raw[i].rt;

		p.buttons = raw[i].buttons;
		if ( p.lt >= trigger )
			p.buttons |= GESS_PADBIT( GESS_PAD_LT );
		if ( p.rt >= trigger )
			p.buttons |= GESS_PADBIT( GESS_PAD_RT );

		p.pressed = p.buttons & ~oldButtons;
		p.released = oldButtons & ~p.buttons;
	}

	// Keep what was learned (not every frame)
	if ( m_bCalDirty && now >= m_flNextCalSave )
		SaveCalibration();
}

//-----------------------------------------------------------------------------
// Automatic dead zones
//
// While a stick is left alone (nothing pressed, the stick near its resting point
// and barely moving), 1.5 seconds of samples give its resting point (subtracted
// from then on) and how far it wanders from it. The dead zone is that wander
// plus a margin, between 0.06 and 0.35. The first measurement after the
// controller is plugged in (or the game starts) replaces the saved one; later
// ones only widen it, so a stick that starts drifting stops creeping. The
// furthest the stick has ever gone from its resting point is its 100%, so worn
// sticks still reach full speed. Movement starts smoothly from the dead zone's
// edge (radial, rescaled).
//-----------------------------------------------------------------------------
ConVar ges_ss_pad_deadzone_auto( "ges_ss_pad_deadzone_auto", "1", FCVAR_ARCHIVE, "Split screen: measure each controller's stick drift and reach and set its dead zones from them (saved in cfg/ges_splitscreen_pads.txt). 0: the fixed ges_ss_pad_deadzone for every stick." );

#define GESS_PADCAL_FILE		"cfg/ges_splitscreen_pads.txt"
#define GESS_REST_SECONDS		1.5
#define GESS_REST_MIN_SAMPLES	20
#define GESS_INNER_MIN			0.06f
#define GESS_INNER_MAX			0.35f
#define GESS_INNER_MARGIN		0.04f

// Added to every measured dead zone after the margin (min 0.06 becomes 0.09): more room so a
// character standing still doesn't creep. Part of the stored value, so it isn't added again
// when the file loads or a stick is measured again. (Was ges_ss_pad_deadzone_add at 0.01;
// renamed when it went up to 0.03 so an archived 0.01 in config.cfg doesn't hold it back.)
static ConVar ges_ss_pad_deadzone_add( "ges_ss_pad_deadzone_add", "0.03", FCVAR_ARCHIVE, "Added to each measured stick dead zone (after the margin; the limits move up with it). Applies to the next measurement." );

static CGESSGamepads::StickCal_t s_EmptyStickCal;

void CGESSGamepads::LoadCalibration()
{
	m_bCalLoaded = true;
	m_Cals.RemoveAll();

	KeyValues *pKV = new KeyValues( "SplitScreenPads" );
	if ( pKV->LoadFromFile( filesystem, GESS_PADCAL_FILE, "MOD" ) )
	{
		for ( KeyValues *pPad = pKV->GetFirstTrueSubKey(); pPad; pPad = pPad->GetNextTrueSubKey() )
		{
			const char *pszId = pPad->GetString( "id", "" );
			if ( !pszId[0] )
				continue;
			PadCal_t &cal = m_Cals[m_Cals.AddToTail()];
			memset( &cal, 0, sizeof( cal ) );
			Q_strncpy( cal.id, pszId, sizeof( cal.id ) );
			Q_strncpy( cal.name, pPad->GetString( "name", "" ), sizeof( cal.name ) );
			for ( int s = 0; s < 2; s++ )
			{
				KeyValues *pStick = pPad->FindKey( s == 0 ? "left" : "right" );
				StickCal_t &sc = cal.stick[s];
				if ( !pStick )
					continue;
				sc.cx = pStick->GetFloat( "rest_x" );
				sc.cy = pStick->GetFloat( "rest_y" );
				sc.noise = pStick->GetFloat( "drift" );
				sc.inner = clamp( pStick->GetFloat( "deadzone" ), GESS_INNER_MIN, GESS_INNER_MAX + Max( 0.0f, ges_ss_pad_deadzone_add.GetFloat() ) );
				sc.reach = clamp( pStick->GetFloat( "reach" ), 0.0f, 1.5f );
				sc.measured = pStick->GetInt( "measured" ) != 0;
			}
		}
	}
	pKV->deleteThis();
}

void CGESSGamepads::SaveCalibration()
{
	m_bCalDirty = false;
	m_flNextCalSave = Plat_FloatTime() + 5.0;

	KeyValues *pKV = new KeyValues( "SplitScreenPads" );
	for ( int i = 0; i < m_Cals.Count(); i++ )
	{
		const PadCal_t &cal = m_Cals[i];
		KeyValues *pPad = pKV->CreateNewKey();
		pPad->SetString( "id", cal.id );
		pPad->SetString( "name", cal.name );
		for ( int s = 0; s < 2; s++ )
		{
			const StickCal_t &sc = cal.stick[s];
			KeyValues *pStick = pPad->FindKey( s == 0 ? "left" : "right", true );
			pStick->SetFloat( "rest_x", sc.cx );
			pStick->SetFloat( "rest_y", sc.cy );
			pStick->SetFloat( "drift", sc.noise );
			pStick->SetFloat( "deadzone", sc.inner );
			pStick->SetFloat( "reach", sc.reach );
			pStick->SetInt( "measured", sc.measured ? 1 : 0 );
		}
	}
	pKV->SaveToFile( filesystem, GESS_PADCAL_FILE, "MOD" );
	pKV->deleteThis();
}

int CGESSGamepads::FindCal( const char *id, const char *name, bool bCreate )
{
	for ( int i = 0; i < m_Cals.Count(); i++ )
	{
		if ( !Q_strcmp( m_Cals[i].id, id ) )
			return i;
	}
	if ( !bCreate )
		return -1;

	PadCal_t &cal = m_Cals[m_Cals.AddToTail()];
	memset( &cal, 0, sizeof( cal ) );
	Q_strncpy( cal.id, id, sizeof( cal.id ) );
	Q_strncpy( cal.name, name, sizeof( cal.name ) );
	for ( int s = 0; s < 2; s++ )
		cal.stick[s].inner = ges_ss_pad_deadzone.GetFloat();
	return m_Cals.Count() - 1;
}

void CGESSGamepads::UpdateCalibration( int index, int stick, float x, float y, bool bIdle )
{
	const int c = m_iPadCal[index];
	if ( c < 0 || !ges_ss_pad_deadzone_auto.GetBool() )
		return;

	StickCal_t &sc = m_Cals[c].stick[stick];
	RestWindow_t &w = m_Rest[index][stick];

	// Reach: the furthest from the resting point (only grows; Recalibrate clears it)
	const float dist = sqrtf( ( x - sc.cx ) * ( x - sc.cx ) + ( y - sc.cy ) * ( y - sc.cy ) );
	if ( dist > sc.reach && dist >= 0.75f )
	{
		sc.reach = MIN( dist, 1.5f );
		m_bCalDirty = true;
	}

	// Resting: near the middle and barely moving since the last poll
	const float fromZero = sqrtf( x * x + y * y );
	const bool bStill = w.count == 0 || ( fabsf( x - w.lastX ) < 0.03f && fabsf( y - w.lastY ) < 0.03f );
	if ( !bIdle || fromZero > 0.4f || !bStill )
	{
		w.count = 0;
		return;
	}

	const double now = Plat_FloatTime();
	if ( w.count == 0 )
	{
		w.start = now;
		w.sumX = w.sumY = 0;
		w.minX = w.maxX = x;
		w.minY = w.maxY = y;
	}
	w.count++;
	w.sumX += x;
	w.sumY += y;
	w.minX = MIN( w.minX, x ); w.maxX = MAX( w.maxX, x );
	w.minY = MIN( w.minY, y ); w.maxY = MAX( w.maxY, y );
	w.lastX = x;
	w.lastY = y;

	if ( now - w.start < GESS_REST_SECONDS || w.count < GESS_REST_MIN_SAMPLES )
		return;

	// A window's worth of rest: where it sits and how far it wanders
	const float mx = w.sumX / w.count, my = w.sumY / w.count;
	const float ex = MAX( w.maxX - mx, mx - w.minX ), ey = MAX( w.maxY - my, my - w.minY );
	const float noise = sqrtf( ex * ex + ey * ey );	// furthest it strayed from where it rests
	w.count = 0;
	if ( noise > 0.15f || sqrtf( mx * mx + my * my ) > 0.2f )
		return;	// It was being touched after all (a resting stick sits near the middle)

	const float inner = clamp( noise + GESS_INNER_MARGIN, GESS_INNER_MIN, GESS_INNER_MAX ) + clamp( ges_ss_pad_deadzone_add.GetFloat(), 0.0f, 0.2f );
	if ( !w.sessionMeasured || !sc.measured )
	{
		// First rest since it was plugged in: measure afresh
		sc.inner = inner;
		sc.noise = noise;
		w.sessionMeasured = true;
	}
	else if ( inner > sc.inner )
	{
		// Later: only ever wider (drift creeping in)
		sc.inner = inner;
		sc.noise = MAX( sc.noise, noise );
	}
	sc.cx = mx;
	sc.cy = my;
	sc.measured = true;
	m_bCalDirty = true;
}

float CGESSGamepads::GetStickOuter( int index, int stick ) const
{
	const StickCal_t &sc = GetStickCal( index, stick );
	return sc.reach >= 0.75f ? MIN( sc.reach, 1.0f ) : 1.0f;
}

void CGESSGamepads::ApplyStick( int index, int stick, float &x, float &y ) const
{
	const int c = m_iPadCal[index];
	if ( c < 0 || !ges_ss_pad_deadzone_auto.GetBool() || !m_Cals[c].stick[stick].measured )
	{
		ApplyRadialDeadzone( x, y, ges_ss_pad_deadzone.GetFloat() );
		return;
	}

	const StickCal_t &sc = m_Cals[c].stick[stick];
	const float dx = x - sc.cx, dy = y - sc.cy;
	const float len = sqrtf( dx * dx + dy * dy );
	if ( len <= sc.inner )
	{
		x = y = 0.0f;
		return;
	}

	// From the dead zone's edge (0) to the stick's reach (1)
	const float outer = MAX( GetStickOuter( index, stick ), sc.inner + 0.1f );
	const float scaled = clamp( ( len - sc.inner ) / ( outer - sc.inner ), 0.0f, 1.0f );
	x = dx / len * scaled;
	y = dy / len * scaled;
}

const char *CGESSGamepads::GetPadName( int index ) const
{
	return index >= 0 && index < GES_SS_MAX_PADS ? m_szPadName[index] : "";
}

const char *CGESSGamepads::GetPadId( int index ) const
{
	return index >= 0 && index < GES_SS_MAX_PADS ? m_szPadId[index] : "";
}

const CGESSGamepads::StickCal_t &CGESSGamepads::GetStickCal( int index, int stick ) const
{
	if ( index < 0 || index >= GES_SS_MAX_PADS || m_iPadCal[index] < 0 || stick < 0 || stick > 1 )
		return s_EmptyStickCal;
	return m_Cals[m_iPadCal[index]].stick[stick];
}

void CGESSGamepads::Recalibrate()
{
	for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
	{
		if ( m_iPadCal[i] < 0 )
			continue;
		for ( int s = 0; s < 2; s++ )
		{
			StickCal_t &sc = m_Cals[m_iPadCal[i]].stick[s];
			memset( &sc, 0, sizeof( sc ) );
			sc.inner = ges_ss_pad_deadzone.GetFloat();
		}
		memset( m_Rest[i], 0, sizeof( m_Rest[i] ) );
	}
	m_bCalDirty = true;
	Msg( "Split screen: controllers recalibrating. Leave the sticks alone for a couple of seconds, then push each one all the way round.\n" );
}

//-----------------------------------------------------------------------------
// One controller's sticks, guided from its card in the Split Screen lobby (R3):
// forget its calibration, measure where the sticks rest (the automatic
// measurement above, about 1.5 s of leaving them alone), then learn how far
// they reach while the player turns them round. Other controllers carry on.
//-----------------------------------------------------------------------------
#define GESS_GUIDED_TURN_SECONDS	8.0		// Time to turn the sticks round before it finishes anyway
#define GESS_GUIDED_DONE_SECONDS	4.0		// How long "Calibrated" stays on the card

void CGESSGamepads::RecalibratePad( int index )
{
	if ( index < 0 || index >= GES_SS_MAX_PADS || m_iPadCal[index] < 0 )
		return;
	for ( int s = 0; s < 2; s++ )
	{
		StickCal_t &sc = m_Cals[m_iPadCal[index]].stick[s];
		memset( &sc, 0, sizeof( sc ) );
		sc.inner = ges_ss_pad_deadzone.GetFloat();
	}
	memset( m_Rest[index], 0, sizeof( m_Rest[index] ) );
	m_iGuidedCal[index] = 1;
	m_flGuidedCalStart[index] = Plat_FloatTime();
	m_bCalDirty = true;
	Msg( "Split screen: controller %d recalibrating its sticks\n", index + 1 );
}

void CGESSGamepads::GetRawStick( int index, int stick, float &x, float &y ) const
{
	x = y = 0.0f;
	if ( index < 0 || index >= GES_SS_MAX_PADS || stick < 0 || stick > 1 )
		return;
	x = m_RawStick[index][stick * 2];
	y = m_RawStick[index][stick * 2 + 1];
}

float CGESSGamepads::GetDeadzoneMin()
{
	return GESS_INNER_MIN;
}

float CGESSGamepads::GetDeadzoneMax()
{
	return GESS_INNER_MAX + clamp( ges_ss_pad_deadzone_add.GetFloat(), 0.0f, 0.2f );
}

void CGESSGamepads::SetStickDeadzone( int index, int stick, float inner )
{
	if ( index < 0 || index >= GES_SS_MAX_PADS || m_iPadCal[index] < 0 || stick < 0 || stick > 1 )
		return;
	StickCal_t &sc = m_Cals[m_iPadCal[index]].stick[stick];
	sc.inner = clamp( inner, GetDeadzoneMin(), GetDeadzoneMax() );
	sc.measured = true;	// Used from now on (a resting point not measured yet is the middle)
	// Kept like a measurement made since it was plugged in: later ones only widen it
	m_Rest[index][stick].sessionMeasured = true;
	m_bCalDirty = true;
}

void CGESSGamepads::RestoreStickCal( int index, int stick, const StickCal_t &cal )
{
	if ( index < 0 || index >= GES_SS_MAX_PADS || m_iPadCal[index] < 0 || stick < 0 || stick > 1 )
		return;
	m_Cals[m_iPadCal[index]].stick[stick] = cal;
	m_iGuidedCal[index] = 0;
	m_bCalDirty = true;
}

void CGESSGamepads::SetPadInMenu( int index, bool bInMenu )
{
	if ( index >= 0 && index < GES_SS_MAX_PADS )
		m_bPadInMenu[index] = bInMenu;
}

bool CGESSGamepads::IsCalibrating( int index ) const
{
	return index >= 0 && index < GES_SS_MAX_PADS && ( m_iGuidedCal[index] == 1 || m_iGuidedCal[index] == 2 );
}

void CGESSGamepads::UpdateGuidedCalibration( int index )
{
	const int c = m_iPadCal[index];
	if ( !m_iGuidedCal[index] || c < 0 )
		return;
	const StickCal_t *sc = m_Cals[c].stick;
	const double now = Plat_FloatTime();
	switch ( m_iGuidedCal[index] )
	{
	case 1:		// Resting: until both sticks have been measured
		if ( !ges_ss_pad_deadzone_auto.GetBool() || ( sc[0].measured && sc[1].measured ) )
		{
			m_iGuidedCal[index] = 2;
			m_flGuidedCalStart[index] = now;
		}
		break;
	case 2:		// Turning: until both have reached the edge, or time's up
		if ( ( sc[0].reach >= 0.75f && sc[1].reach >= 0.75f ) || now - m_flGuidedCalStart[index] >= GESS_GUIDED_TURN_SECONDS )
		{
			m_iGuidedCal[index] = 3;
			m_flGuidedCalStart[index] = now;
			SaveCalibration();
			Msg( "Split screen: controller %d calibrated (dead zones: left %.2f, right %.2f)\n", index + 1, sc[0].inner, sc[1].inner );
		}
		break;
	case 3:
		if ( now - m_flGuidedCalStart[index] >= GESS_GUIDED_DONE_SECONDS )
			m_iGuidedCal[index] = 0;
		break;
	}
}

const char *CGESSGamepads::GetCalibrationPrompt( int index, char *buf, int bufLen ) const
{
	if ( index < 0 || index >= GES_SS_MAX_PADS || m_iPadCal[index] < 0 )
		return "";
	const StickCal_t *sc = m_Cals[m_iPadCal[index]].stick;
	switch ( m_iGuidedCal[index] )
	{
	case 1:
		Q_strncpy( buf, "Calibrating: leave both sticks centred...", bufLen );
		break;
	case 2:
		Q_snprintf( buf, bufLen, "Now turn both sticks all the way round (%s%s)",
			sc[0].reach >= 0.75f ? "left done" : "left", sc[1].reach >= 0.75f ? ", right done" : ", right" );
		break;
	case 3:
		Q_snprintf( buf, bufLen, "Calibrated - dead zones: left %.2f, right %.2f", sc[0].inner, sc[1].inner );
		break;
	default:
		Q_strncpy( buf, "Press R3 to calibrate analog sticks", bufLen );
		break;
	}
	return buf;
}

CON_COMMAND( ges_ss_pad_recalibrate, "Forget the connected controllers' measured stick dead zones and measure them again. With a controller number, only that one (like R3 in the Split Screen lobby)" )
{
	if ( args.ArgC() > 1 )
		GESSGamepads()->RecalibratePad( atoi( args[1] ) - 1 );
	else
		GESSGamepads()->Recalibrate();
}

CON_COMMAND( ges_ss_pad_calibration, "Show each connected controller's measured stick dead zones" )
{
	CGESSGamepads *pPads = GESSGamepads();
	for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
	{
		if ( !pPads->GetPad( i ).connected )
			continue;
		Msg( "Controller %d: %s [%s]\n", i + 1, pPads->GetPadName( i ), pPads->GetPadId( i ) );
		for ( int s = 0; s < 2; s++ )
		{
			const CGESSGamepads::StickCal_t &sc = pPads->GetStickCal( i, s );
			Msg( "  %s stick: %s rest %+.3f %+.3f, drift %.3f, dead zone %.3f, reach %.3f (100%% at %.3f)\n", s == 0 ? "left " : "right",
				sc.measured ? "measured" : "measuring", sc.cx, sc.cy, sc.noise, sc.measured ? sc.inner : ges_ss_pad_deadzone.GetFloat(),
				sc.reach, pPads->GetStickOuter( i, s ) );
		}
	}
}

const GESSPadState_t &CGESSGamepads::GetPad( int index ) const
{
	if ( index < 0 || index >= GES_SS_MAX_PADS )
		return s_EmptyPad;
	return m_Pads[index];
}

int CGESSGamepads::GetConnectedCount() const
{
	int count = 0;
	for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
	{
		if ( m_Pads[i].connected )
			count++;
	}
	return count;
}

int CGESSGamepads::GetMaxPads() const
{
	return m_pBackend ? m_pBackend->MaxPads() : 0;
}

const char *CGESSGamepads::GetBackendName() const
{
	return m_pBackend ? m_pBackend->Name() : "none";
}

void CGESSGamepads::ConsumeEdges( int index )
{
	if ( index < 0 || index >= GES_SS_MAX_PADS )
		return;
	m_Pads[index].pressed = 0;
	m_Pads[index].released = 0;
}
