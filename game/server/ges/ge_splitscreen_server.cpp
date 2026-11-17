///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Server (GES)
//   File        : ge_splitscreen_server.cpp
//   Description :
//      Server side of local split-screen multiplayer. See ge_splitscreen_shared.h
//
////////////////////////////////////////////////////////////////////////////

#include "cbase.h"
#include "ge_splitscreen_server.h"
#include "gemp_player.h"
#include "gemp_gamerules.h"
#include "ge_character_data.h"
#include "usercmd.h"
#include "checksum_md5.h"
#include "viewport_panel_names.h"
#include "ge_gameplay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sv_ges_splitscreen( "sv_ges_splitscreen", "1", FCVAR_NOTIFY, "Allow clients to bring local split screen guest players into the game." );
ConVar sv_ges_splitscreen_maxguests( "sv_ges_splitscreen_maxguests", "7", FCVAR_NOTIFY, "Maximum number of split screen guests per client (0-7).", true, 0, true, GES_SS_MAX_GUESTS );
ConVar sv_ges_splitscreen_ownertimeout( "sv_ges_splitscreen_ownertimeout", "30", 0, "Seconds a guest stays on the server after its owner disappears." );
static ConVar ges_ss_suicide_instant_respawn( "ges_ss_suicide_instant_respawn", "0", FCVAR_ARCHIVE, "1: Commit Suicide in a split screen player's START menu respawns them straight away, skipping the death screen. 0: they respawn as after any death." );

static CGESplitScreenServer g_GESplitScreenServer;
CGESplitScreenServer *GESplitScreenServer() { return &g_GESplitScreenServer; }

CGESplitScreenServer::CGESplitScreenServer() : CAutoGameSystemPerFrame( "CGESplitScreenServer" )
{
	m_bCreatingGuest = false;
	m_flLevelStartTime = 0.0f;
	m_iPendingOwnerUserId = -1;
	m_iPendingSlot = 0;
	m_szPendingChar[0] = '\0';
	m_iPendingTeam = 0;
}

//-----------------------------------------------------------------------------
// Record helpers
//-----------------------------------------------------------------------------
CGESplitScreenServer::GuestRecord_t *CGESplitScreenServer::FindRecordByGuest( int guestUserId )
{
	for ( int i = 0; i < m_Guests.Count(); i++ )
	{
		if ( m_Guests[i].guestUserId == guestUserId )
			return &m_Guests[i];
	}
	return NULL;
}

CGESplitScreenServer::GuestRecord_t *CGESplitScreenServer::FindRecord( int ownerUserId, int slot )
{
	for ( int i = 0; i < m_Guests.Count(); i++ )
	{
		if ( m_Guests[i].ownerUserId == ownerUserId && m_Guests[i].slot == slot )
			return &m_Guests[i];
	}
	return NULL;
}

void CGESplitScreenServer::RemoveRecord( int guestUserId )
{
	for ( int i = m_Guests.Count() - 1; i >= 0; i-- )
	{
		if ( m_Guests[i].guestUserId == guestUserId )
			m_Guests.Remove( i );
	}
}

void CGESplitScreenServer::KickGuest( int guestUserId, const char *reason )
{
	engine->ServerCommand( UTIL_VarArgs( "kickid %d %s\n", guestUserId, reason ) );
}

CGEMPPlayer *CGESplitScreenServer::GetGuest( CBasePlayer *pHost, int slot )
{
	if ( !pHost )
		return NULL;

	GuestRecord_t *pRec = FindRecord( pHost->GetUserID(), slot );
	if ( !pRec )
		return NULL;

	CGEMPPlayer *pGuest = ToGEMPPlayer( UTIL_PlayerByUserId( pRec->guestUserId ) );
	if ( !pGuest || !pGuest->IsSplitGuest() )
		return NULL;

	return pGuest;
}

int CGESplitScreenServer::GetGuestCount( CBasePlayer *pHost )
{
	if ( !pHost )
		return 0;

	int count = 0;
	for ( int i = 0; i < m_Guests.Count(); i++ )
	{
		if ( m_Guests[i].ownerUserId == pHost->GetUserID() )
			count++;
	}
	return count;
}

bool CGESplitScreenServer::IsGuest( CBasePlayer *pPlayer )
{
	return pPlayer && FindRecordByGuest( pPlayer->GetUserID() ) != NULL;
}

bool CGESplitScreenServer::UsesController( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsPlayer() )
		return false;
	if ( IsGuest( pPlayer ) )
		return true;
	if ( pPlayer->IsFakeClient() )
		return false;
	const char *pszPad = engine->GetClientConVarValue( pPlayer->entindex(), "cl_ges_ss_pad" );
	return pszPad && atoi( pszPad ) != 0;
}

//-----------------------------------------------------------------------------
// Creation
//-----------------------------------------------------------------------------
bool CGESplitScreenServer::OnClientPutInServer( edict_t *pEdict )
{
	if ( m_bCreatingGuest )
		return true;

	// Guests survive level changes as fake clients, recognise them again
	return FindRecordByGuest( engine->GetPlayerUserId( pEdict ) ) != NULL;
}

void CGESplitScreenServer::OnGuestCreated( CGEMPPlayer *pGuest )
{
	if ( !pGuest )
		return;

	const int userid = pGuest->GetUserID();
	GuestRecord_t *pRec = FindRecordByGuest( userid );

	if ( !pRec && m_bCreatingGuest )
	{
		GuestRecord_t rec;
		rec.guestUserId = userid;
		rec.ownerUserId = m_iPendingOwnerUserId;
		rec.slot = m_iPendingSlot;
		Q_strncpy( rec.character, m_szPendingChar, sizeof( rec.character ) );
		rec.team = m_iPendingTeam;
		rec.bJoinedGame = false;
		rec.flOwnerMissingSince = -1.0f;
		m_Guests.AddToTail( rec );
		pRec = &m_Guests.Tail();
	}

	if ( !pRec )
		return;

	// After a level change the guest has to join the game again
	pRec->bJoinedGame = false;

	CBasePlayer *pOwner = UTIL_PlayerByUserId( pRec->ownerUserId );
	pGuest->SetSplitGuest( pOwner ? pOwner->entindex() : 0, pRec->slot );
}

bool CGESplitScreenServer::CreateGuest( CGEMPPlayer *pHost, int slot, const char *name, const char *character, int team )
{
	char szName[MAX_PLAYER_NAME_LENGTH];
	Q_strncpy( szName, ( name && name[0] ) ? name : UTIL_VarArgs( "Player %d", slot + 1 ), sizeof( szName ) );

	// Strip characters that would break console commands (kickid etc.)
	for ( char *c = szName; *c; c++ )
	{
		if ( *c == '"' || *c == ';' || *c == '%' || *c == '\n' || *c == '\r' )
			*c = ' ';
	}

	m_bCreatingGuest = true;
	m_iPendingOwnerUserId = pHost->GetUserID();
	m_iPendingSlot = slot;
	Q_strncpy( m_szPendingChar, character ? character : CHAR_RANDOM_IDENT, sizeof( m_szPendingChar ) );
	m_iPendingTeam = team;

	edict_t *pEdict = engine->CreateFakeClient( szName );

	m_bCreatingGuest = false;
	m_iPendingOwnerUserId = -1;

	if ( !pEdict )
	{
		ClientPrint( pHost, HUD_PRINTTALK, "Split screen: the server is full, player %s1 could not join.", UTIL_VarArgs( "%d", slot + 1 ) );
		return false;
	}

	return true;
}

void CGESplitScreenServer::JoinGuestToGame( CGEMPPlayer *pGuest, GuestRecord_t *pRec )
{
	pRec->bJoinedGame = true;

	// Join the team they picked (or auto assign), exactly as a normal player would
	// through the team menu
	const int current = pGuest->GetTeamNumber();
	if ( GEMPRules()->IsTeamplay() )
	{
		const bool bChosen = pRec->team == TEAM_MI6 || pRec->team == TEAM_JANUS;
		if ( current == TEAM_SPECTATOR || current == TEAM_UNASSIGNED || ( bChosen && current != pRec->team ) )
			pGuest->HandleCommand_JoinTeam( bChosen ? pRec->team : MAX_GE_TEAMS );
	}
	else if ( current == TEAM_SPECTATOR || current == TEAM_UNASSIGNED )
	{
		pGuest->HandleCommand_JoinTeam( TEAM_UNASSIGNED );
	}

	ApplyGuestCharacter( pGuest, pRec );
}

void CGESplitScreenServer::ApplyGuestCharacter( CGEMPPlayer *pGuest, GuestRecord_t *pRec )
{
	const char *character = pRec->character;
	const CGECharData *pChar = GECharacters()->Get( character );

	// In teamplay the profile's character might belong to the other team
	if ( !pChar || ( GEMPRules()->IsTeamplay() && pChar->iTeam != pGuest->GetTeamNumber() ) )
		character = CHAR_RANDOM_IDENT;

	pGuest->SetPlayerModel( character );
}

//-----------------------------------------------------------------------------
// Commands sent by the owning client
//   ges_ss_join <slot> <name> <character>
//   ges_ss_leave <slot>
//   ges_ss_setchar <slot> <character>
//-----------------------------------------------------------------------------
bool CGESplitScreenServer::ClientCommand( CGEMPPlayer *pPlayer, const CCommand &args )
{
	const char *cmd = args[0];

	const bool bJoin = !Q_stricmp( cmd, GES_SS_CMD_JOIN );
	const bool bLeave = !Q_stricmp( cmd, GES_SS_CMD_LEAVE );
	const bool bSetChar = !Q_stricmp( cmd, GES_SS_CMD_SETCHAR );
	const bool bTeam = !Q_stricmp( cmd, GES_SS_CMD_TEAM );
	const bool bHost = !Q_stricmp( cmd, GES_SS_CMD_HOST );

	if ( !Q_stricmp( cmd, "ges_ss_endmatch" ) )
	{
		// Player 1 of a split screen game on their own server left the match: it ends
		// for everyone and shows the results (the match report)
		if ( pPlayer && !pPlayer->IsSplitGuest() && pPlayer->IsSplitHost() && !engine->IsDedicatedServer() &&
			pPlayer == UTIL_GetListenServerHost() && GEGameplay() && !GEGameplay()->IsInFinalIntermission() )
		{
			GEGameplay()->EndMatch();
		}
		return true;
	}

	const bool bSuicide = !Q_stricmp( cmd, GES_SS_CMD_SUICIDE );
	if ( !bJoin && !bLeave && !bSetChar && !bTeam && !bHost && !bSuicide )
		return false;

	// Guests can't own guests
	if ( !pPlayer || pPlayer->IsSplitGuest() || pPlayer->IsBotPlayer() )
		return true;

	if ( bSuicide )
	{
		// ges_ss_suicide <slot>: Commit Suicide in that player's START menu. Only that
		// player dies, the same way as the online game's "kill" (the game mode scores
		// it like any suicide: 1 point off in DeathMatch), and respawns as after any death.
		const int slot = args.ArgC() >= 2 ? atoi( args[1] ) : -1;
		CGEMPPlayer *pVictim = NULL;
		if ( slot == 0 )
			pVictim = pPlayer;
		else if ( slot >= 1 && slot <= GES_SS_MAX_GUESTS && FindRecord( pPlayer->GetUserID(), slot ) )
			pVictim = GetGuest( pPlayer, slot );
		if ( pVictim && pVictim->IsAlive() && !pVictim->IsObserver() )
		{
			pVictim->CommitSuicide();
			if ( ges_ss_suicide_instant_respawn.GetBool() && !pVictim->IsAlive() )
				pVictim->ForceRespawn();
		}
		return true;
	}

	if ( bHost )
	{
		// ges_ss_host <1/0> <character>: player 1 of a split screen game, who gets
		// their team / character from their profile instead of GoldenEye's menus
		const bool bIsHost = args.ArgC() >= 2 && atoi( args[1] ) != 0;
		pPlayer->SetSplitHost( bIsHost, args.ArgC() >= 3 ? args[2] : CHAR_RANDOM_IDENT );
		if ( bIsHost )
		{
			pPlayer->ShowViewPortPanel( PANEL_TEAM, false );
			pPlayer->ShowViewPortPanel( PANEL_CHARSELECT, false );
		}
		return true;
	}

	if ( args.ArgC() < 2 )
		return true;

	const int slot = atoi( args[1] );
	if ( slot < 1 || slot > GES_SS_MAX_GUESTS )
		return true;

	GuestRecord_t *pRec = FindRecord( pPlayer->GetUserID(), slot );

	if ( bLeave )
	{
		if ( pRec )
		{
			int guestUserId = pRec->guestUserId;
			RemoveRecord( guestUserId );
			KickGuest( guestUserId, "Left split screen" );
		}
		return true;
	}

	if ( bSetChar )
	{
		// ges_ss_update <slot> <name> <character>: the player picked another profile
		CGEMPPlayer *pGuest = GetGuest( pPlayer, slot );
		if ( pRec && pGuest && args.ArgC() >= 4 )
		{
			char szName[MAX_PLAYER_NAME_LENGTH];
			Q_strncpy( szName, args[2], sizeof( szName ) );
			for ( char *c = szName; *c; c++ )
			{
				if ( *c == '"' || *c == ';' || *c == '%' || *c == '\n' || *c == '\r' )
					*c = ' ';
			}
			if ( szName[0] && Q_strcmp( szName, pGuest->GetPlayerName() ) )
				engine->SetFakeClientConVarValue( pGuest->edict(), "name", szName );

			Q_strncpy( pRec->character, args[3], sizeof( pRec->character ) );
			pGuest->SetPlayerModel( pRec->character );
		}
		return true;
	}

	if ( bTeam )
	{
		// ges_ss_team <slot> <team>: the player picked a team (after a profile)
		CGEMPPlayer *pGuest = GetGuest( pPlayer, slot );
		if ( pRec && args.ArgC() >= 3 )
		{
			pRec->team = atoi( args[2] );
			if ( pGuest && pRec->bJoinedGame && GEMPRules()->IsTeamplay() )
			{
				const int current = pGuest->GetTeamNumber();
				const bool bChosen = pRec->team == TEAM_MI6 || pRec->team == TEAM_JANUS;
				if ( bChosen ? current != pRec->team : ( current != TEAM_MI6 && current != TEAM_JANUS ) )
					pGuest->HandleCommand_JoinTeam( bChosen ? pRec->team : MAX_GE_TEAMS );
				ApplyGuestCharacter( pGuest, pRec );
			}
		}
		return true;
	}

	// Join: ges_ss_join <slot> <name> <character> <team>
	const int team = args.ArgC() >= 5 ? atoi( args[4] ) : 0;

	// A record whose guest is gone: the engine drops fake clients when the map
	// changes (e.g. the next map after a match), so after the new map has had a
	// moment to bring back anyone it kept, forget it and create the guest again.
	// (Before, the join was taken as "already here" and the players never came back.)
	if ( pRec && !UTIL_PlayerByUserId( pRec->guestUserId ) && gpGlobals->curtime - m_flLevelStartTime > 2.0f )
	{
		DevMsg( "Split screen: player %d of %s was dropped with the map change, creating them again\n", slot + 1, pPlayer->GetPlayerName() );
		RemoveRecord( pRec->guestUserId );
		pRec = NULL;
	}

	if ( pRec )
	{
		// Already here, just update the character / team choice
		if ( args.ArgC() >= 4 )
			Q_strncpy( pRec->character, args[3], sizeof( pRec->character ) );
		if ( args.ArgC() >= 5 )
			pRec->team = team;
		return true;
	}

	if ( !sv_ges_splitscreen.GetBool() )
	{
		ClientPrint( pPlayer, HUD_PRINTTALK, "Split screen players are disabled on this server." );
		return true;
	}

	if ( GetGuestCount( pPlayer ) >= sv_ges_splitscreen_maxguests.GetInt() )
	{
		ClientPrint( pPlayer, HUD_PRINTTALK, "This server does not allow any more split screen players." );
		return true;
	}

	CreateGuest( pPlayer, slot, args.ArgC() >= 3 ? args[2] : NULL, args.ArgC() >= 4 ? args[3] : CHAR_RANDOM_IDENT, team );
	return true;
}

void CGESplitScreenServer::OnClientDisconnected( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	const int userid = pPlayer->GetUserID();

	// A guest left (kicked), forget about it
	RemoveRecord( userid );

	// An owner left, take its guests with it
	for ( int i = m_Guests.Count() - 1; i >= 0; i-- )
	{
		if ( m_Guests[i].ownerUserId == userid )
		{
			int guestUserId = m_Guests[i].guestUserId;
			m_Guests.Remove( i );
			KickGuest( guestUserId, "Owner left" );
		}
	}
}

//-----------------------------------------------------------------------------
// Input routing
//-----------------------------------------------------------------------------
void CGESplitScreenServer::ProcessHostUsercmds( CGEMPPlayer *pHost, CUserCmd *cmds, int numcmds, int totalcmds, int dropped_packets, bool paused )
{
	if ( !pHost || totalcmds <= 0 || m_Guests.Count() == 0 )
		return;

	CUtlVector<CUserCmd> guestCmds;
	guestCmds.SetCount( totalcmds );

	for ( int slot = 1; slot <= GES_SS_MAX_GUESTS; slot++ )
	{
		CGEMPPlayer *pGuest = GetGuest( pHost, slot );
		if ( !pGuest )
			continue;

		const int resetSerial = pGuest->GetSplitAngleReset() & GES_SS_ANGLERESET_MASK;

		for ( int i = 0; i < totalcmds; i++ )
		{
			const CUserCmd &src = cmds[i];
			const CGESplitGuestCmd &g = src.splitguests[slot - 1];
			CUserCmd &dst = guestCmds[i];

			dst.Reset();
			dst.command_number = src.command_number;
			dst.tick_count = src.tick_count;
			dst.random_seed = MD5_PseudoRandom( src.command_number * GES_SS_MAX_PLAYERS + slot ) & 0x7fffffff;

			// Until the client confirms it saw our last angle reset, keep the server angles
			if ( g.active && g.anglereset == resetSerial )
				dst.viewangles = g.viewangles;
			else
				dst.viewangles = pGuest->EyeAngles();

			if ( g.active )
			{
				dst.forwardmove = g.forwardmove;
				dst.sidemove = g.sidemove;
				dst.buttons = g.buttons;
				dst.hostweaponcycle = g.weaponcycle;
			}
		}

		pGuest->ProcessUsercmds( guestCmds.Base(), numcmds, totalcmds, dropped_packets, paused );
	}
}

void CGESplitScreenServer::AddGuestsToPVS( CGEMPPlayer *pHost )
{
	if ( !pHost || m_Guests.Count() == 0 )
		return;

	for ( int slot = 1; slot <= GES_SS_MAX_GUESTS; slot++ )
	{
		CGEMPPlayer *pGuest = GetGuest( pHost, slot );
		if ( pGuest )
			engine->AddOriginToPVS( pGuest->EyePosition() );
	}
}

//-----------------------------------------------------------------------------
// Per frame upkeep
//-----------------------------------------------------------------------------
void CGESplitScreenServer::LevelInitPostEntity()
{
	m_flLevelStartTime = gpGlobals->curtime;

	for ( int i = 0; i < m_Guests.Count(); i++ )
	{
		m_Guests[i].bJoinedGame = false;
		m_Guests[i].flOwnerMissingSince = -1.0f;
	}
}

void CGESplitScreenServer::FrameUpdatePostEntityThink()
{
	for ( int i = m_Guests.Count() - 1; i >= 0; i-- )
	{
		GuestRecord_t &rec = m_Guests[i];

		CGEMPPlayer *pGuest = ToGEMPPlayer( UTIL_PlayerByUserId( rec.guestUserId ) );
		if ( !pGuest )
			continue;	// Not (re)connected yet

		CGEMPPlayer *pOwner = ToGEMPPlayer( UTIL_PlayerByUserId( rec.ownerUserId ) );
		if ( !pOwner || !pOwner->IsConnected() )
		{
			if ( rec.flOwnerMissingSince < 0 )
				rec.flOwnerMissingSince = gpGlobals->curtime;
			else if ( gpGlobals->curtime - rec.flOwnerMissingSince > sv_ges_splitscreen_ownertimeout.GetFloat() )
			{
				int guestUserId = rec.guestUserId;
				m_Guests.Remove( i );
				KickGuest( guestUserId, "Owner timed out" );
			}
			continue;
		}

		rec.flOwnerMissingSince = -1.0f;

		// Keep the owner binding fresh (entity indices can change on level change)
		pGuest->SetSplitGuest( pOwner->entindex(), rec.slot );

		// Guests use the owner's interpolation so lag compensation lines up with what they see
		pGuest->m_bLagCompensation = pOwner->m_bLagCompensation;
		pGuest->m_fLerpTime = pOwner->m_fLerpTime;

		if ( !rec.bJoinedGame && pGuest->IsConnected() )
			JoinGuestToGame( pGuest, &rec );

		pGuest->UpdateSplitGuestState();
	}
}

CON_COMMAND_F( ges_ss_debug_kill, "Testing: ges_ss_debug_kill <split screen player 1-8> [damage] kills (or with a damage amount, hurts) that player", FCVAR_DONTRECORD )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() || args.ArgC() < 2 )
		return;

	CBasePlayer *pHost = UTIL_GetListenServerHost();
	const int slot = atoi( args[1] ) - 1;
	CBasePlayer *pPlayer = slot == 0 ? pHost : GESplitScreenServer()->GetGuest( pHost, slot );
	if ( !pPlayer || !pPlayer->IsAlive() )
	{
		Msg( "ges_ss_debug_kill: no living player %d\n", slot + 1 );
		return;
	}
	// Optional damage amount to just hurt them (for the health / armour display)
	const float flDamage = args.ArgC() > 2 ? atof( args[2] ) : 1000.0f;
	CTakeDamageInfo info( GetContainingEntity( INDEXENT( 0 ) ), GetContainingEntity( INDEXENT( 0 ) ), flDamage, DMG_GENERIC );
	pPlayer->TakeDamage( info );
	Msg( "ges_ss_debug_kill: %s player %d (%s), health %d armour %d\n", pPlayer->IsAlive() ? "hurt" : "killed", slot + 1, pPlayer->GetPlayerName(),
		pPlayer->GetHealth(), pPlayer->ArmorValue() );
}

CON_COMMAND_F( ges_ss_debug_switch, "Testing: ges_ss_debug_switch <split screen player 1-8> switches that player to their next weapon", FCVAR_DONTRECORD )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() || args.ArgC() < 2 )
		return;

	CBasePlayer *pHost = UTIL_GetListenServerHost();
	const int slot = atoi( args[1] ) - 1;
	CBasePlayer *pPlayer = slot == 0 ? pHost : GESplitScreenServer()->GetGuest( pHost, slot );
	if ( !pPlayer || !pPlayer->IsAlive() )
		return;

	// The next weapon they carry after the active one
	CBaseCombatWeapon *pActive = pPlayer->GetActiveWeapon();
	int start = 0;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( pPlayer->GetWeapon( i ) == pActive )
			start = i;
	}
	for ( int n = 1; n <= MAX_WEAPONS; n++ )
	{
		CBaseCombatWeapon *pWeapon = pPlayer->GetWeapon( ( start + n ) % MAX_WEAPONS );
		if ( pWeapon && pWeapon != pActive && pPlayer->Weapon_Switch( pWeapon ) )
		{
			Msg( "ges_ss_debug_switch: player %d now has %s\n", slot + 1, pWeapon->GetClassname() );
			return;
		}
	}
}

CON_COMMAND_F( ges_ss_debug_server_viewmodels, "Testing: list the server's player viewmodels", FCVAR_DONTRECORD )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer )
			continue;
		for ( int v = 0; v < MAX_VIEWMODELS; v++ )
		{
			CBaseViewModel *pVM = pPlayer->GetViewModel( v );
			Msg( "server: player %d (%s) viewmodel %d: ", i, pPlayer->GetPlayerName(), v );
			if ( !pVM )
			{
				Msg( "none\n" );
				continue;
			}
			Msg( "entity %d model %s%s owner %d weapon %d\n", pVM->entindex(), STRING( pVM->GetModelName() ),
				pVM->IsEffectActive( EF_NODRAW ) ? " NODRAW" : "", pVM->GetOwner() ? pVM->GetOwner()->entindex() : -1,
				pVM->GetOwningWeapon() ? pVM->GetOwningWeapon()->entindex() : -1 );
		}
	}
}
