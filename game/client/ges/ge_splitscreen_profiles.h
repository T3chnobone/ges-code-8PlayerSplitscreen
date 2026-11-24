///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_splitscreen_profiles.h
//   Description :
//      Local player profiles for split screen. Profiles are created and
//      edited from the main menu and picked by players when they join.
//      Stored in cfg/ges_splitscreen_profiles.txt
//
////////////////////////////////////////////////////////////////////////////

#ifndef GE_SPLITSCREEN_PROFILES_H
#define GE_SPLITSCREEN_PROFILES_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

#define GES_SS_MAX_PROFILES			16
#define GES_SS_PROFILE_NAME_LEN		32
#define GES_SS_PROFILE_CHAR_LEN		96	// Fits a custom character spec (ge_customchar.h)

#define GES_SS_SENS_MIN		0.1f
#define GES_SS_SENS_MAX		3.0f

struct GESSProfile_t
{
	GESSProfile_t();

	char	name[GES_SS_PROFILE_NAME_LEN];
	char	character[GES_SS_PROFILE_CHAR_LEN];	// Character identifier or "_random"
	float	sensitivity;	// Look speed multiplier for the controller
	bool	invertY;		// Inverted look
};

class CGESSProfiles
{
public:
	void Load();
	void Save();

	int Count() const						{ return m_Profiles.Count(); }
	GESSProfile_t *Get( int i )				{ return m_Profiles.IsValidIndex( i ) ? &m_Profiles[i] : NULL; }
	const GESSProfile_t *Get( int i ) const	{ return m_Profiles.IsValidIndex( i ) ? &m_Profiles[i] : NULL; }

	// Returns the new index or -1 when full
	int  Add( const char *name = NULL );
	void Remove( int i );

	// A profile used when there are no profiles or the player picks "Guest"
	static const GESSProfile_t &DefaultProfile();

private:
	CUtlVector<GESSProfile_t> m_Profiles;
};

CGESSProfiles *GESSProfiles();

#endif // GE_SPLITSCREEN_PROFILES_H
