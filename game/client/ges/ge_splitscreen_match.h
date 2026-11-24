///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_splitscreen_match.h
//   Description :
//      Match settings for a local split screen game: map, game mode (the
//      same scenarios as online play), teams, and how the match is won
//      (timed score, first to N kills, or best kill/death ratio).
//      Saved to cfg/ges_splitscreen_match.txt.
//
////////////////////////////////////////////////////////////////////////////

#ifndef GE_SPLITSCREEN_MATCH_H
#define GE_SPLITSCREEN_MATCH_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

enum GESSTeamSupport_t
{
	GESS_TEAMS_NEVER = 0,
	GESS_TEAMS_OPTIONAL,
	GESS_TEAMS_ALWAYS,
};

struct GESSGameMode_t
{
	char ident[32];		// Scenario file name, the value of ge_gameplay
	char name[64];		// Friendly name for the menu
	GESSTeamSupport_t teams;
	bool ownWeapons;	// The mode hands out its own weapons (weapon set doesn't apply)
};

// A weapon set from scripts/loadouts (the same sets online servers use,
// e.g. Power Weapons or Proximity Mines). ident is the value of ge_weaponset.
struct GESSWeaponSet_t
{
	char ident[64];
	char name[64];
};

#define GESS_WEAPONSET_RANDOM	"random_loadout"

// Cheats (the Split Screen menu's Cheats page). Each is an on / off server setting that the
// match sets when it starts and saves as "cheat_<key>". To add one: a line here (and raise
// GESS_NUM_CHEATS), a replicated ConVar for it, and the code that uses it.
struct GESSCheatDef_t
{
	const char *key;		// Saved as "cheat_<key>" in ges_splitscreen_match.txt
	const char *label;		// Shown on the Cheats page
	const char *help;		// One line under it
	const char *cvar;		// The server setting it turns on
};
#define GESS_NUM_CHEATS		2
#define GESS_MAX_CHEATS		16
extern const GESSCheatDef_t g_GESSCheats[GESS_NUM_CHEATS];

// The Cheats page's Jump height and Turbo mode choices: a name and the cvar value
struct GESSChoice_t
{
	const char *name;
	float value;
};
#define GESS_NUM_JUMPHEIGHTS	3
#define GESS_NUM_TURBOMODES		4
#define GESS_TURBO_NORMAL		1	// g_GESSTurboModes' 1x speed
extern const GESSChoice_t g_GESSJumpHeights[GESS_NUM_JUMPHEIGHTS];	// ge_ss_jumpheight
extern const GESSChoice_t g_GESSTurboModes[GESS_NUM_TURBOMODES];	// ge_velocity

struct GESSMatchSettings_t
{
	char map[64];
	char mode[32];
	char weaponset[64];	// ge_weaponset value, GESS_WEAPONSET_RANDOM for a random set each round
	bool teams;
	bool jump;		// ge_allowjump: players can jump
	int  jumpheight;	// ge_ss_jumpheight (Cheats page): index into g_GESSJumpHeights
	int  turbo;		// ge_velocity (Cheats page, the online Turbo mode): index into g_GESSTurboModes
	bool radar;		// ge_allowradar: the radar shows
	int  aimassist;	// ge_ss_aimassist: 0 off, 1 classic (shots home in), 2 sticky, 3 slowdown (controller players)
	bool cheats[GESS_MAX_CHEATS];	// g_GESSCheats on / off
	int  wincondition;	// GEWinCondition_t: 0 score, 1 kills, 2 kill/death ratio
	int  timelimit;		// Minutes, 0 = no limit (only allowed with the kills win condition)
	int  killlimit;		// Kills to win with wincondition 1
	int  perdisplay;	// Players per display (1-8): the screen is split into this many equal views
};

#define GESS_MATCH_TIME_MAX		60
#define GESS_MATCH_KILLS_MAX	999

class CGESplitScreenMatch
{
public:
	CGESplitScreenMatch();

	void Load();
	void EnsureLoaded()					{ if ( !m_bLoaded ) Load(); }
	void Save();

	GESSMatchSettings_t &Settings()		{ return m_Settings; }

	// Game modes found in python/ges/GamePlay (plus the known built-in ones)
	void RefreshModes();
	int  GetModeCount() const						{ return m_Modes.Count(); }
	const GESSGameMode_t &GetMode( int i ) const	{ return m_Modes[i]; }
	int  FindMode( const char *ident ) const;

	// Weapon sets from scripts/loadouts/weapon_sets_*.txt, default sets first.
	// The first entry is always "Random".
	void RefreshWeaponSets();
	int  GetWeaponSetCount() const						{ return m_WeaponSets.Count(); }
	const GESSWeaponSet_t &GetWeaponSet( int i ) const	{ return m_WeaponSets[i]; }
	int  FindWeaponSet( const char *ident ) const;
	void AddWeaponSet( const char *ident, const char *name, bool bDefaultGroup );

	// Map names found in maps/*.bsp
	void RefreshMaps( CUtlVector<CUtlString> &maps ) const;

	// Make the settings valid (team support, limits)
	void Validate();

	// Set the server cvars and load the map
	void Start();

	// Scenario config files can change the limits when they load, so the
	// limits are applied once more, a second after the map has loaded.
	void OnLevelInit();
	void Update();

	// A match started from the Split Screen menu (or ges_splitscreen_match_start)
	// is running on our own server. Set by Start(), cleared once we leave that
	// server (back to the menus for a while, or connected somewhere else).
	bool IsMatchActive() const			{ return m_bMatchActive; }

	// Players take their screens one by one when the match's first map loads
	// (the game waits for them). Set by Start(), cleared once everyone is in.
	bool IsPregamePending() const		{ return m_bMatchActive && m_bPregamePending; }
	void ClearPregamePending()			{ m_bPregamePending = false; }

private:
	void ApplyCvars( bool bMode );

	bool m_bMatchActive;
	bool m_bPregamePending;
	float m_flDisconnectedSince;

	GESSMatchSettings_t m_Settings;
	CUtlVector<GESSGameMode_t> m_Modes;
	CUtlVector<GESSWeaponSet_t> m_WeaponSets;
	int m_iDefaultSetsEnd;
	bool m_bPendingApply;
	bool m_bLoaded;
	float m_flApplyTime;
};

CGESplitScreenMatch *GESSMatch();

#endif // GE_SPLITSCREEN_MATCH_H
