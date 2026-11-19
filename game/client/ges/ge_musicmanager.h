///////////// Copyright � 2011, Goldeneye: Source. All rights reserved. /////////////
// 
// File: ge_musicmanager.h
// Description:
//      Loads music from "scripts/level_music_LEVELNAME.txt" in KeyValue form
//		and controls the playback and looping throughout the GE:S experience
//
// Created On: 18 Sep 11
// Created By: Jonathan White <killermonkey> 
/////////////////////////////////////////////////////////////////////////////
#ifndef GE_MUSICMANAGER_H
#define GE_MUSICMANAGER_H
#pragma once

#include "GameEventListener.h"
#include "tier0/threadtools.h"
#include "igamesystem.h"

#ifdef _WIN32
#include "ge_fmodex.h" // FMOD Ex (fmodex.dll), loaded at runtime
#endif

#define GE_MUSIC_MENU		"_menu"
#define GE_MUSIC_DEFAULT	"_default"

class CGEMusicManager : public CThread, public CAutoGameSystem
{
public:
	// Playlists by soundscape name
	typedef CUtlDict<CUtlVector<char*>*, int> PlaylistDict_t;

	CGEMusicManager();
	~CGEMusicManager();

	// Generic controls to the music player
	void SetVolume( float vol );
	void PauseMusic();
	void ResumeMusic();

	const char *CurrentPlaylist() { return m_szPlayList; }
	void LoadPlayList( const char *levelname );

	// Soundscape transition
	void SetSoundscape( const char *soundscape );

	// Game Event Functions (these drive the thread actions)
	void PostInit();
	void LevelInitPreEntity();
	
	void StopThread() { m_bRun = false; }

	// Debug functions
	void DEBUG_NextSong();

protected:
	// Thread controls
	int Run();
	void ClearPlayList();

	void StartFade( int type );
	void FadeThink();
	void PauseThink();
	void NextSong();
#ifdef _WIN32
	void EndSong( FMOD::Channel *pChannel );
#endif

	// Playlist functions
	void InternalLoadPlaylist();
	void ReadPlaylistFile( const char *playlist, PlaylistDict_t &playlists );
	void EnforcePlaylist( bool force = false );
	static void AddSong( PlaylistDict_t &playlists, const char* relative, const char* soundscape );
	static void ClearPlaylists( PlaylistDict_t &playlists );
	static void MovePlaylists( PlaylistDict_t &from, PlaylistDict_t &to );

	// Soundscape functions
	void InternalLoadSoundscape();
	
	void CheckWindowFocus();

	enum {
		STATE_STOPPED = 0,
		STATE_PAUSED,
		STATE_UNPAUSE,
		STATE_PLAYING,
	};

private:
	// Status flags (thread transient)
	bool	m_bRun;
	bool	m_bFMODRunning;
	bool	m_bLoadPlaylist;
	bool	m_bVolumeDirty;
	int		m_iState;
	// Soundscape transitions
	char	m_szSoundscape[64];
	bool	m_bLoadSoundscape;

	// Music playlist controls
	char	m_szPlayList[64];
	int		m_nCurrSongIdx;
	bool	m_bNewPlaylist;

	// Playlist seperated by soundscape name
	PlaylistDict_t	m_Playlists;
	// Read by LoadPlayList (main thread), swapped in by the music thread (under m_Lock)
	PlaylistDict_t	m_PendingPlaylists;
	CUtlVector<char*>* m_CurrPlaylist;

	// Global sound controls
	float	m_fVolume;
	float	m_fNextSongTime;
	float	m_fFadeStartTime;
	float	m_fPauseTime;

	// Fade Status
	enum {
		FADE_NONE,
		FADE_IN,	// m_pCurrSong IN, m_pLastSong OUT
		FADE_OUT,	// m_pCurrSong and m_pLastSong OUT
	};

	int		m_iFadeMode;
	float	m_fLastFadeTime;
	float	m_fFadeInPercent;
	float	m_fFadeOutPercent;

	// Current song length in milliseconds
	unsigned int m_iCurrSongLength;

#ifdef _WIN32
	// FMOD Variables
	FMOD::System		*m_pSystem;
	FMOD::Channel		*m_pLastSong;
	FMOD::Channel		*m_pCurrSong;
	FMOD::ChannelGroup	*m_pMasterChannel;
#endif
};

extern void StartGEMusicManager();
extern void StopGEMusicManager();
extern CGEMusicManager *GEMusicManager();

#endif
