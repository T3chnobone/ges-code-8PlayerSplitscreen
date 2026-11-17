///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_splitscreen_match.cpp
//   Description :
//      Match settings for a local split screen game. See the header.
//
////////////////////////////////////////////////////////////////////////////

#include "cbase.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "script_parser.h"
#include "ge_splitscreen_match.h"
#include "ge_splitscreen_shared.h"
#include "inetchannelinfo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Paintball cheat's death screen colouring (used by c_baseviewmodel.cpp)
ConVar ges_paint_deathscreen_style( "ges_paint_deathscreen_style", "0", 0,
	"Paintball cheat's death screen: 0 the paint colours mixed, 1 one random paint colour per death." );

#define GESS_MATCH_FILE		"cfg/ges_splitscreen_match.txt"
#define GESS_MATCH_PYDIR	"python/ges/GamePlay/*.py"

// Players a split screen server makes room for (8 local players plus some online)
#define GESS_MATCH_MAXPLAYERS	16

// The modes that ship with the game, with friendly names and team support.
// Modes found on disk that aren't listed here are added with their file name.
static const GESSGameMode_t s_KnownModes[] =
{
	{ "DeathMatch",			"Deathmatch",						GESS_TEAMS_OPTIONAL, false },
	{ "MWGG",				"The Man with the Golden Gun",		GESS_TEAMS_NEVER, false },
	{ "LTK",				"License to Kill",					GESS_TEAMS_OPTIONAL, false },
	{ "YOLT",				"You Only Live Twice",				GESS_TEAMS_OPTIONAL, false },
	{ "Arsenal",			"Arsenal",							GESS_TEAMS_NEVER, false },	// Levels through the set's weapons
	{ "GunTrade",			"Gun Trade",						GESS_TEAMS_OPTIONAL, true },
	{ "DrNoArmor",			"Dr. No Armor",						GESS_TEAMS_OPTIONAL, false },
	{ "ViewToAKill",		"A View to a Kill",					GESS_TEAMS_NEVER, false },
	{ "LivingDaylights",	"The Living Daylights",				GESS_TEAMS_OPTIONAL, false },
	{ "CaptureTheFlag",		"Capture the Key (teams)",			GESS_TEAMS_ALWAYS, false },
	{ "TournamentDM",		"Tournament Deathmatch (teams)",	GESS_TEAMS_ALWAYS, false },
};

// The Cheats page (see GESSCheatDef_t). Add new cheats at the end.
const GESSCheatDef_t g_GESSCheats[GESS_NUM_CHEATS] =
{
	{ "bighead", "Big Head", "Every character's head is drawn twice as big, and its hitbox grows with it.", "ge_cheat_bighead" },
	{ "paintball", "Paintball", "Shots leave splats of mixed paint colours on walls and props instead of bullet holes.", "ge_cheat_paintball" },
};

static CGESplitScreenMatch s_GESSMatch;
CGESplitScreenMatch *GESSMatch() { return &s_GESSMatch; }

CGESplitScreenMatch::CGESplitScreenMatch()
{
	Q_strncpy( m_Settings.map, "ge_archives", sizeof( m_Settings.map ) );
	Q_strncpy( m_Settings.mode, "DeathMatch", sizeof( m_Settings.mode ) );
	Q_strncpy( m_Settings.weaponset, GESS_WEAPONSET_RANDOM, sizeof( m_Settings.weaponset ) );
	m_Settings.teams = false;
	m_Settings.jump = true;
	m_Settings.radar = true;
	m_Settings.aimassist = 0;
	for ( int i = 0; i < GESS_MAX_CHEATS; i++ )
		m_Settings.cheats[i] = false;
	m_Settings.wincondition = 0;
	m_Settings.timelimit = 10;
	m_Settings.killlimit = 20;
	m_Settings.perdisplay = 4;
	m_bPendingApply = false;
	m_bLoaded = false;
	m_flApplyTime = 0;
	m_iDefaultSetsEnd = 0;
	m_bMatchActive = false;
	m_bPregamePending = false;
	m_flDisconnectedSince = 0;
}

void CGESplitScreenMatch::Load()
{
	KeyValues *pKV = new KeyValues( "SplitScreenMatch" );
	if ( pKV->LoadFromFile( filesystem, GESS_MATCH_FILE, "MOD" ) )
	{
		Q_strncpy( m_Settings.map, pKV->GetString( "map", m_Settings.map ), sizeof( m_Settings.map ) );
		Q_strncpy( m_Settings.mode, pKV->GetString( "mode", m_Settings.mode ), sizeof( m_Settings.mode ) );
		Q_strncpy( m_Settings.weaponset, pKV->GetString( "weaponset", m_Settings.weaponset ), sizeof( m_Settings.weaponset ) );
		m_Settings.teams = pKV->GetBool( "teams", m_Settings.teams );
		m_Settings.jump = pKV->GetBool( "jump", m_Settings.jump );
		m_Settings.radar = pKV->GetBool( "radar", m_Settings.radar );
		m_Settings.aimassist = pKV->GetInt( "aimassist", m_Settings.aimassist );
		for ( int i = 0; i < GESS_NUM_CHEATS; i++ )
			m_Settings.cheats[i] = pKV->GetBool( VarArgs( "cheat_%s", g_GESSCheats[i].key ), m_Settings.cheats[i] );
		m_Settings.wincondition = pKV->GetInt( "wincondition", m_Settings.wincondition );
		m_Settings.timelimit = pKV->GetInt( "timelimit", m_Settings.timelimit );
		m_Settings.killlimit = pKV->GetInt( "killlimit", m_Settings.killlimit );
		m_Settings.perdisplay = pKV->GetInt( "perdisplay", m_Settings.perdisplay );
	}
	pKV->deleteThis();

	m_bLoaded = true;
	RefreshModes();
	RefreshWeaponSets();
	Validate();
}

void CGESplitScreenMatch::Save()
{
	Validate();

	KeyValues *pKV = new KeyValues( "SplitScreenMatch" );
	pKV->SetString( "map", m_Settings.map );
	pKV->SetString( "mode", m_Settings.mode );
	pKV->SetString( "weaponset", m_Settings.weaponset );
	pKV->SetBool( "teams", m_Settings.teams );
	pKV->SetBool( "jump", m_Settings.jump );
	pKV->SetBool( "radar", m_Settings.radar );
	pKV->SetInt( "aimassist", m_Settings.aimassist );
	for ( int i = 0; i < GESS_NUM_CHEATS; i++ )
		pKV->SetBool( VarArgs( "cheat_%s", g_GESSCheats[i].key ), m_Settings.cheats[i] );
	pKV->SetInt( "wincondition", m_Settings.wincondition );
	pKV->SetInt( "timelimit", m_Settings.timelimit );
	pKV->SetInt( "killlimit", m_Settings.killlimit );
	pKV->SetInt( "perdisplay", m_Settings.perdisplay );
	pKV->SaveToFile( filesystem, GESS_MATCH_FILE, "MOD" );
	pKV->deleteThis();
}

int CGESplitScreenMatch::FindMode( const char *ident ) const
{
	for ( int i = 0; i < m_Modes.Count(); i++ )
	{
		if ( !Q_stricmp( m_Modes[i].ident, ident ) )
			return i;
	}
	return -1;
}

void CGESplitScreenMatch::RefreshModes()
{
	m_Modes.RemoveAll();

	// Which scenario files are installed
	CUtlVector<CUtlString> files;
	FileFindHandle_t findHandle;
	const char *pFilename = filesystem->FindFirstEx( GESS_MATCH_PYDIR, "MOD", &findHandle );
	while ( pFilename )
	{
		if ( !Q_stristr( pFilename, "__init__" ) )
		{
			char base[32];
			Q_FileBase( pFilename, base, sizeof( base ) );
			files.AddToTail( CUtlString( base ) );
		}
		pFilename = filesystem->FindNext( findHandle );
	}
	filesystem->FindClose( findHandle );

	// Known modes first, in a sensible order. If no scenario files were found
	// (e.g. packed in a VPK), offer all of the known ones anyway.
	for ( int i = 0; i < ARRAYSIZE( s_KnownModes ); i++ )
	{
		bool bInstalled = files.Count() == 0;
		for ( int f = 0; f < files.Count() && !bInstalled; f++ )
			bInstalled = !Q_stricmp( files[f].Get(), s_KnownModes[i].ident );

		if ( bInstalled )
			m_Modes.AddToTail( s_KnownModes[i] );
	}

	// Custom scenarios
	for ( int f = 0; f < files.Count(); f++ )
	{
		if ( FindMode( files[f].Get() ) >= 0 )
			continue;

		GESSGameMode_t mode;
		Q_strncpy( mode.ident, files[f].Get(), sizeof( mode.ident ) );
		Q_strncpy( mode.name, files[f].Get(), sizeof( mode.name ) );
		mode.teams = GESS_TEAMS_OPTIONAL;
		mode.ownWeapons = false;
		m_Modes.AddToTail( mode );
	}
}

// Reads the weapon set files the same way the server's loadout manager does
class CGESSWeaponSetParser : public CScriptParser
{
public:
	CGESSWeaponSetParser( CGESplitScreenMatch *pMatch ) : m_pMatch( pMatch ) {}

protected:
	virtual void Parse( KeyValues *pKeyValuesData, const char *szFileWithoutEXT )
	{
		// Each file holds groups of sets; groups ending in _mhide are hidden from menus
		const char *group = pKeyValuesData->GetName();
		if ( Q_strstr( group, "_mhide" ) )
			return;

		const bool bDefault = !Q_stricmp( group, "Default Sets" );
		for ( KeyValues *pSet = pKeyValuesData->GetFirstSubKey(); pSet; pSet = pSet->GetNextKey() )
		{
			const char *ident = pSet->GetName();
			if ( Q_strstr( ident, " " ) || Q_strstr( ident, "," ) )
				continue;
			m_pMatch->AddWeaponSet( ident, pSet->GetString( "print_name", ident ), bDefault );
		}
	}

private:
	CGESplitScreenMatch *m_pMatch;
};

void CGESplitScreenMatch::AddWeaponSet( const char *ident, const char *name, bool bDefaultGroup )
{
	GESSWeaponSet_t set;
	Q_strncpy( set.ident, ident, sizeof( set.ident ) );
	Q_strncpy( set.name, name, sizeof( set.name ) );

	// A custom set with the same name replaces the default one
	const int existing = FindWeaponSet( ident );
	if ( existing > 0 )
	{
		m_WeaponSets[existing] = set;
		return;
	}

	// Default sets are listed before custom ones
	if ( bDefaultGroup )
		m_WeaponSets.InsertBefore( m_iDefaultSetsEnd++, set );
	else
		m_WeaponSets.AddToTail( set );
}

int CGESplitScreenMatch::FindWeaponSet( const char *ident ) const
{
	for ( int i = 0; i < m_WeaponSets.Count(); i++ )
	{
		if ( !Q_stricmp( m_WeaponSets[i].ident, ident ) )
			return i;
	}
	return -1;
}

void CGESplitScreenMatch::RefreshWeaponSets()
{
	m_WeaponSets.RemoveAll();

	GESSWeaponSet_t random;
	Q_strncpy( random.ident, GESS_WEAPONSET_RANDOM, sizeof( random.ident ) );
	Q_strncpy( random.name, "Random (a new set each round)", sizeof( random.name ) );
	m_WeaponSets.AddToTail( random );
	m_iDefaultSetsEnd = 1;

	CGESSWeaponSetParser parser( this );
	parser.InitParser( "scripts/loadouts/weapon_sets_default.X" );
	parser.SetHasBeenParsed( false );
	parser.InitParser( "scripts/loadouts/weapon_sets_custom*.X" );
}

void CGESplitScreenMatch::RefreshMaps( CUtlVector<CUtlString> &maps ) const
{
	maps.RemoveAll();

	FileFindHandle_t findHandle;
	const char *pFilename = filesystem->FindFirstEx( "maps/*.bsp", "MOD", &findHandle );
	while ( pFilename )
	{
		// Skip the transition map, it's not a real level
		if ( Q_stricmp( pFilename, "ge_transition.bsp" ) )
		{
			char base[64];
			Q_FileBase( pFilename, base, sizeof( base ) );
			maps.AddToTail( CUtlString( base ) );
		}
		pFilename = filesystem->FindNext( findHandle );
	}
	filesystem->FindClose( findHandle );
}

void CGESplitScreenMatch::Validate()
{
	m_Settings.wincondition = clamp( m_Settings.wincondition, 0, 2 );
	m_Settings.aimassist = clamp( m_Settings.aimassist, 0, 3 );
	m_Settings.timelimit = clamp( m_Settings.timelimit, 0, GESS_MATCH_TIME_MAX );
	m_Settings.killlimit = clamp( m_Settings.killlimit, 1, GESS_MATCH_KILLS_MAX );
	m_Settings.perdisplay = clamp( m_Settings.perdisplay, 1, GES_SS_MAX_PLAYERS );

	// Only "first to N kills" can go without a time limit, everything else needs one to end
	if ( m_Settings.wincondition != 1 && m_Settings.timelimit < 1 )
		m_Settings.timelimit = 10;

	if ( m_WeaponSets.Count() > 0 && FindWeaponSet( m_Settings.weaponset ) < 0 )
		Q_strncpy( m_Settings.weaponset, GESS_WEAPONSET_RANDOM, sizeof( m_Settings.weaponset ) );

	int mode = FindMode( m_Settings.mode );
	if ( mode < 0 && m_Modes.Count() > 0 )
	{
		mode = 0;
		Q_strncpy( m_Settings.mode, m_Modes[0].ident, sizeof( m_Settings.mode ) );
	}

	if ( mode >= 0 )
	{
		if ( m_Modes[mode].teams == GESS_TEAMS_NEVER )
			m_Settings.teams = false;
		else if ( m_Modes[mode].teams == GESS_TEAMS_ALWAYS )
			m_Settings.teams = true;
	}
}

void CGESplitScreenMatch::ApplyCvars( bool bMode )
{
	if ( bMode )
	{
		// Keep this mode for the whole session instead of rotating scenarios
		engine->ClientCmd_Unrestricted( "ge_gameplay_mode 0\n" );
		engine->ClientCmd_Unrestricted( VarArgs( "ge_gameplay \"%s\"\n", m_Settings.mode ) );
	}

	// Which weapons are on the map (a mode with its own weapons ignores this).
	// Set again after loading too, in case a map or mode config picked another set.
	engine->ClientCmd_Unrestricted( VarArgs( "ge_weaponset \"%s\"\n", m_Settings.weaponset ) );

	// Teams are what the players picked, not decided by the player count
	engine->ClientCmd_Unrestricted( "ge_autoautoteam 0\n" );
	engine->ClientCmd_Unrestricted( "ge_autoteam 0\n" );
	engine->ClientCmd_Unrestricted( VarArgs( "ge_teamplay %d\n", m_Settings.teams ? 1 : 0 ) );

	// One round lasting the whole match, so the time limit is the match length
	engine->ClientCmd_Unrestricted( "ge_roundcount 0\n" );
	engine->ClientCmd_Unrestricted( "ge_roundtime 0\n" );
	engine->ClientCmd_Unrestricted( VarArgs( "mp_timelimit %d\n", m_Settings.timelimit ) );

	engine->ClientCmd_Unrestricted( VarArgs( "ge_wincondition %d\n", m_Settings.wincondition ) );
	engine->ClientCmd_Unrestricted( VarArgs( "ge_killlimit %d\n", m_Settings.wincondition == 1 ? m_Settings.killlimit : 0 ) );

	// Jumping on or off, for everyone: it's the server's movement rule, so the
	// guests obey it too
	engine->ClientCmd_Unrestricted( VarArgs( "ge_allowjump %d\n", m_Settings.jump ? 1 : 0 ) );

	// The radar, for everyone (the server's setting; a game mode can still force it on)
	engine->ClientCmd_Unrestricted( VarArgs( "ge_allowradar %d\n", m_Settings.radar ? 1 : 0 ) );
	// Aim assist for the players on controllers (split screen only; keyboard and mouse never get it)
	engine->ClientCmd_Unrestricted( VarArgs( "ge_ss_aimassist %d\n", m_Settings.aimassist ) );
	// Cheats (the Cheats page), for everyone
	for ( int i = 0; i < GESS_NUM_CHEATS; i++ )
		engine->ClientCmd_Unrestricted( VarArgs( "%s %d\n", g_GESSCheats[i].cvar, m_Settings.cheats[i] ? 1 : 0 ) );
}

void CGESplitScreenMatch::Start()
{
	if ( !m_bLoaded )
		Load();

	Validate();
	Save();

	// Room for all the local players. maxplayers can only change while no
	// server is running; when already in a game the current value is kept.
	engine->ClientCmd_Unrestricted( VarArgs( "maxplayers %d\n", GESS_MATCH_MAXPLAYERS ) );
	ApplyCvars( true );
	engine->ClientCmd_Unrestricted( VarArgs( "map \"%s\"\n", m_Settings.map ) );

	m_bPendingApply = true;
	m_flApplyTime = 0;

	// From here on GoldenEye's own team / character menus are replaced by the
	// players' profiles, even before anyone else joins
	m_bMatchActive = true;
	m_bPregamePending = true;
	m_flDisconnectedSince = 0;
}

void CGESplitScreenMatch::OnLevelInit()
{
	// The scenario's own config runs while the map loads and may change the
	// limits, so set ours again now that we're in. Only for a match we started.
	if ( m_bPendingApply )
		m_flApplyTime = gpGlobals->realtime + 1.0f;

	// Joined someone else's server: that's online play, not our match
	INetChannelInfo *pNet = engine->GetNetChannelInfo();
	if ( m_bMatchActive && pNet && !pNet->IsLoopback() )
	{
		m_bMatchActive = false;
		m_bPendingApply = false;
	}
}

void CGESplitScreenMatch::Update()
{
	// The match is over once we're back in the menus (not just loading the next map)
	if ( m_bMatchActive )
	{
		if ( engine->IsConnected() || engine->IsDrawingLoadingImage() )
		{
			m_flDisconnectedSince = 0;
		}
		else if ( m_flDisconnectedSince <= 0 )
		{
			m_flDisconnectedSince = Plat_FloatTime();
		}
		else if ( Plat_FloatTime() - m_flDisconnectedSince > 3.0 )
		{
			m_bMatchActive = false;
			m_bPendingApply = false;
			m_flDisconnectedSince = 0;
		}
	}
	if ( !m_bPendingApply || m_flApplyTime <= 0 || gpGlobals->realtime < m_flApplyTime )
		return;

	m_bPendingApply = false;
	m_flApplyTime = 0;
	ApplyCvars( false );
}

CON_COMMAND( ges_splitscreen_match_start, "Start a split screen match with the saved match settings" )
{
	GESSMatch()->Start();
}
