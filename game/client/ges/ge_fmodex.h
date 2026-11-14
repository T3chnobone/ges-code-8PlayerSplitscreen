/////////////////////////////////////////////////////////////////////////////
//
// File: ge_fmodex.h
// Description:
//      Minimal runtime binding to FMOD Ex (fmodex.dll, the FMOD the game ships).
//      fmodex.dll is loaded with LoadLibrary and its C API is resolved with GetProcAddress,
//      so client.dll neither links an FMOD import library nor needs FMOD SDK headers.
//      The wrapper classes mirror the FMOD C++ method names that ge_musicmanager uses.
//
/////////////////////////////////////////////////////////////////////////////
#ifndef GE_FMODEX_H
#define GE_FMODEX_H
#pragma once

#ifdef _WIN32

// FMOD Ex types and constants (values from FMOD Ex 4.x fmod.h)
typedef int FMOD_RESULT;
typedef unsigned int FMOD_MODE;
typedef unsigned int FMOD_INITFLAGS;
typedef unsigned int FMOD_TIMEUNIT;

#define FMOD_OK				0
#define FMOD_DEFAULT		0x00000000
#define FMOD_INIT_NORMAL	0x00000000
#define FMOD_TIMEUNIT_MS	0x00000001
#define FMOD_CHANNEL_FREE	-1

namespace FMOD
{
	class System;
	class Channel;
	class ChannelGroup;
	class Sound;
}

// Function table filled from fmodex.dll
struct GEFMODExAPI_t
{
	FMOD_RESULT ( __stdcall *System_Create )( FMOD::System **system );
	FMOD_RESULT ( __stdcall *System_GetVersion )( FMOD::System *system, unsigned int *version );
	FMOD_RESULT ( __stdcall *System_Init )( FMOD::System *system, int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata );
	FMOD_RESULT ( __stdcall *System_GetMasterChannelGroup )( FMOD::System *system, FMOD::ChannelGroup **channelgroup );
	FMOD_RESULT ( __stdcall *System_CreateStream )( FMOD::System *system, const char *name_or_data, FMOD_MODE mode, void *exinfo, FMOD::Sound **sound );
	FMOD_RESULT ( __stdcall *System_PlaySound )( FMOD::System *system, int channelid, FMOD::Sound *sound, int paused, FMOD::Channel **channel );
	FMOD_RESULT ( __stdcall *System_Update )( FMOD::System *system );
	FMOD_RESULT ( __stdcall *System_Release )( FMOD::System *system );
	FMOD_RESULT ( __stdcall *Channel_SetVolume )( FMOD::Channel *channel, float volume );
	FMOD_RESULT ( __stdcall *Channel_SetPaused )( FMOD::Channel *channel, int paused );
	FMOD_RESULT ( __stdcall *Channel_GetPosition )( FMOD::Channel *channel, unsigned int *position, FMOD_TIMEUNIT postype );
	FMOD_RESULT ( __stdcall *Channel_GetCurrentSound )( FMOD::Channel *channel, FMOD::Sound **sound );
	FMOD_RESULT ( __stdcall *Channel_SetChannelGroup )( FMOD::Channel *channel, FMOD::ChannelGroup *channelgroup );
	FMOD_RESULT ( __stdcall *ChannelGroup_SetVolume )( FMOD::ChannelGroup *channelgroup, float volume );
	FMOD_RESULT ( __stdcall *ChannelGroup_SetMute )( FMOD::ChannelGroup *channelgroup, int mute );
	FMOD_RESULT ( __stdcall *ChannelGroup_Stop )( FMOD::ChannelGroup *channelgroup );
	FMOD_RESULT ( __stdcall *Sound_GetLength )( FMOD::Sound *sound, unsigned int *length, FMOD_TIMEUNIT lengthtype );
	FMOD_RESULT ( __stdcall *Sound_Release )( FMOD::Sound *sound );
};

extern GEFMODExAPI_t g_FMODEx;

// Loads fmodex.dll (from client.dll's folder first) and resolves the API.
// Returns false, after printing a warning, if the DLL or any function is missing.
bool GE_LoadFMODEx();

namespace FMOD
{
	inline FMOD_RESULT System_Create( System **system ) { return g_FMODEx.System_Create( system ); }

	// Opaque handles: these objects are never constructed, only pointers from fmodex.dll are used
	class System
	{
	public:
		FMOD_RESULT init( int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata ) { return g_FMODEx.System_Init( this, maxchannels, flags, extradriverdata ); }
		FMOD_RESULT getVersion( unsigned int *version ) { return g_FMODEx.System_GetVersion( this, version ); }
		FMOD_RESULT getMasterChannelGroup( ChannelGroup **channelgroup ) { return g_FMODEx.System_GetMasterChannelGroup( this, channelgroup ); }
		FMOD_RESULT createStream( const char *name, FMOD_MODE mode, void *exinfo, Sound **sound ) { return g_FMODEx.System_CreateStream( this, name, mode, exinfo, sound ); }
		// FMOD Ex has no channel-group argument on playSound: play on a free channel, then assign the group
		FMOD_RESULT playSound( Sound *sound, ChannelGroup *channelgroup, bool paused, Channel **channel )
		{
			FMOD_RESULT r = g_FMODEx.System_PlaySound( this, FMOD_CHANNEL_FREE, sound, paused ? 1 : 0, channel );
			if ( r == FMOD_OK && channelgroup && channel && *channel )
				g_FMODEx.Channel_SetChannelGroup( *channel, channelgroup );
			return r;
		}
		FMOD_RESULT update() { return g_FMODEx.System_Update( this ); }
		FMOD_RESULT release() { return g_FMODEx.System_Release( this ); }
	};

	class Channel
	{
	public:
		FMOD_RESULT setVolume( float volume ) { return g_FMODEx.Channel_SetVolume( this, volume ); }
		FMOD_RESULT setPaused( bool paused ) { return g_FMODEx.Channel_SetPaused( this, paused ? 1 : 0 ); }
		FMOD_RESULT getPosition( unsigned int *position, FMOD_TIMEUNIT postype ) { return g_FMODEx.Channel_GetPosition( this, position, postype ); }
		FMOD_RESULT getCurrentSound( Sound **sound ) { return g_FMODEx.Channel_GetCurrentSound( this, sound ); }
	};

	class ChannelGroup
	{
	public:
		FMOD_RESULT setVolume( float volume ) { return g_FMODEx.ChannelGroup_SetVolume( this, volume ); }
		FMOD_RESULT setMute( bool mute ) { return g_FMODEx.ChannelGroup_SetMute( this, mute ? 1 : 0 ); }
		FMOD_RESULT stop() { return g_FMODEx.ChannelGroup_Stop( this ); }
	};

	class Sound
	{
	public:
		FMOD_RESULT getLength( unsigned int *length, FMOD_TIMEUNIT lengthtype ) { return g_FMODEx.Sound_GetLength( this, length, lengthtype ); }
		FMOD_RESULT release() { return g_FMODEx.Sound_Release( this ); }
	};
}

#endif // _WIN32
#endif // GE_FMODEX_H
