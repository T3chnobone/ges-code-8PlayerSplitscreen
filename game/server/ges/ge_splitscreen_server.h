///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Server (GES)
//   File        : ge_splitscreen_server.h
//   Description :
//      Server side of local split-screen multiplayer. Creates "guest" players
//      (fake clients) that belong to a real client and feeds them the input
//      that the owning client sends inside its own usercmd stream.
//
////////////////////////////////////////////////////////////////////////////

#ifndef GE_SPLITSCREEN_SERVER_H
#define GE_SPLITSCREEN_SERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"
#include "ge_splitscreen_shared.h"
#include "ge_character_data.h"

class CGEMPPlayer;
class CUserCmd;
class CCommand;

class CGESplitScreenServer : public CAutoGameSystemPerFrame
{
public:
	CGESplitScreenServer();

	// CAutoGameSystemPerFrame
	virtual void LevelInitPostEntity();
	virtual void FrameUpdatePostEntityThink();

	// Called from ClientPutInServer. Returns true if this edict is a split screen guest
	// and should be created as a normal player instead of a bot.
	bool OnClientPutInServer( edict_t *pEdict );

	// Called once the guest's player entity exists to bind it to its owner
	void OnGuestCreated( CGEMPPlayer *pGuest );

	// Called from CGEMPPlayer::ClientCommand, returns true if handled
	bool ClientCommand( CGEMPPlayer *pPlayer, const CCommand &args );

	// Called when any client leaves the server
	void OnClientDisconnected( CBasePlayer *pPlayer );

	// Called from CGEMPPlayer::ProcessUsercmds after the host's own commands are queued
	void ProcessHostUsercmds( CGEMPPlayer *pHost, CUserCmd *cmds, int numcmds, int totalcmds, int dropped_packets, bool paused );

	// Add the PVS of all of a host's guests to the host's PVS
	void AddGuestsToPVS( CGEMPPlayer *pHost );

	// Find the guest player for this host and slot (1..GES_SS_MAX_GUESTS)
	CGEMPPlayer *GetGuest( CBasePlayer *pHost, int slot );
	int GetGuestCount( CBasePlayer *pHost );

	// A split screen guest of a player on this server
	bool IsGuest( CBasePlayer *pPlayer );
	// Played with a controller in a split screen game: every guest, and a host whose
	// split screen says player 1 is on a controller (cl_ges_ss_pad). Aim assist is only for these.
	bool UsesController( CBasePlayer *pPlayer );

private:
	struct GuestRecord_t
	{
		int		guestUserId;
		int		ownerUserId;
		int		slot;
		char	character[MAX_CHAR_IDENT];
		int		team;			// Chosen team (TEAM_MI6 / TEAM_JANUS), MAX_GE_TEAMS or 0 = auto assign
		bool	bJoinedGame;
		float	flOwnerMissingSince;
	};

	GuestRecord_t *FindRecordByGuest( int guestUserId );
	GuestRecord_t *FindRecord( int ownerUserId, int slot );
	void RemoveRecord( int guestUserId );
	void KickGuest( int guestUserId, const char *reason );
	void JoinGuestToGame( CGEMPPlayer *pGuest, GuestRecord_t *pRec );

	bool CreateGuest( CGEMPPlayer *pHost, int slot, const char *name, const char *character, int team );
	void ApplyGuestCharacter( CGEMPPlayer *pGuest, GuestRecord_t *pRec );

	CUtlVector<GuestRecord_t> m_Guests;

	// Set while we are inside engine->CreateFakeClient for a guest
	bool	m_bCreatingGuest;
	float	m_flLevelStartTime;	// When the current map started (guests dropped by the map change)
	int		m_iPendingOwnerUserId;
	int		m_iPendingSlot;
	char	m_szPendingChar[MAX_CHAR_IDENT];
	int		m_iPendingTeam;
};

CGESplitScreenServer *GESplitScreenServer();

#endif // GE_SPLITSCREEN_SERVER_H
