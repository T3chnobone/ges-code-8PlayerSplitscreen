/////////////////////////////////////////////////////////////////////////////
//
// File: ge_fmodex.cpp
// Description:
//      See Header
//
/////////////////////////////////////////////////////////////////////////////
#include "cbase.h"

#ifdef _WIN32
#pragma warning(disable:4005)
#include <windows.h>
#include "ge_fmodex.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

GEFMODExAPI_t g_FMODEx;

static HMODULE	s_hFMODEx = NULL;
static bool		s_bTriedLoad = false;
static bool		s_bLoaded = false;

// Look for fmodex.dll next to client.dll (gesource\bin) first
static HMODULE LoadFMODExDLL()
{
	HMODULE hSelf = NULL;
	char szPath[MAX_PATH];
	if ( GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR) &LoadFMODExDLL, &hSelf ) && GetModuleFileNameA( hSelf, szPath, sizeof( szPath ) ) )
	{
		char *pSlash = strrchr( szPath, '\\' );
		if ( pSlash )
		{
			*( pSlash + 1 ) = 0;
			V_strncat( szPath, "fmodex.dll", sizeof( szPath ) );
			HMODULE h = LoadLibraryExA( szPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
			if ( h )
				return h;
		}
	}

	return LoadLibraryA( "fmodex.dll" );
}

bool GE_LoadFMODEx()
{
	if ( s_bTriedLoad )
		return s_bLoaded;
	s_bTriedLoad = true;

	s_hFMODEx = LoadFMODExDLL();
	if ( !s_hFMODEx )
	{
		Warning( "[FMOD] fmodex.dll not found, music is disabled\n" );
		return false;
	}

	bool bOK = true;
#define GE_FMODEX_BIND( field, name ) \
	*(FARPROC *) &g_FMODEx.field = GetProcAddress( s_hFMODEx, name ); \
	if ( !g_FMODEx.field ) { Warning( "[FMOD] fmodex.dll is missing %s, music is disabled\n", name ); bOK = false; }

	GE_FMODEX_BIND( System_Create,					"FMOD_System_Create" );
	GE_FMODEX_BIND( System_GetVersion,				"FMOD_System_GetVersion" );
	GE_FMODEX_BIND( System_Init,					"FMOD_System_Init" );
	GE_FMODEX_BIND( System_GetMasterChannelGroup,	"FMOD_System_GetMasterChannelGroup" );
	GE_FMODEX_BIND( System_CreateStream,			"FMOD_System_CreateStream" );
	GE_FMODEX_BIND( System_PlaySound,				"FMOD_System_PlaySound" );
	GE_FMODEX_BIND( System_Update,					"FMOD_System_Update" );
	GE_FMODEX_BIND( System_Release,					"FMOD_System_Release" );
	GE_FMODEX_BIND( Channel_SetVolume,				"FMOD_Channel_SetVolume" );
	GE_FMODEX_BIND( Channel_SetPaused,				"FMOD_Channel_SetPaused" );
	GE_FMODEX_BIND( Channel_GetPosition,			"FMOD_Channel_GetPosition" );
	GE_FMODEX_BIND( Channel_GetCurrentSound,		"FMOD_Channel_GetCurrentSound" );
	GE_FMODEX_BIND( Channel_SetChannelGroup,		"FMOD_Channel_SetChannelGroup" );
	GE_FMODEX_BIND( ChannelGroup_SetVolume,			"FMOD_ChannelGroup_SetVolume" );
	GE_FMODEX_BIND( ChannelGroup_SetMute,			"FMOD_ChannelGroup_SetMute" );
	GE_FMODEX_BIND( ChannelGroup_Stop,				"FMOD_ChannelGroup_Stop" );
	GE_FMODEX_BIND( Sound_GetLength,				"FMOD_Sound_GetLength" );
	GE_FMODEX_BIND( Sound_Release,					"FMOD_Sound_Release" );
#undef GE_FMODEX_BIND

	if ( !bOK )
	{
		FreeLibrary( s_hFMODEx );
		s_hFMODEx = NULL;
		return false;
	}

	s_bLoaded = true;
	DevMsg( 2, "[FMOD] Loaded fmodex.dll\n" );
	return true;
}

#endif // _WIN32
