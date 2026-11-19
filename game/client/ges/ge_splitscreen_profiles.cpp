///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_splitscreen_profiles.cpp
//   Description :
//      Local player profiles for split screen. See ge_splitscreen_profiles.h
//
////////////////////////////////////////////////////////////////////////////

#include "cbase.h"
#include "ge_splitscreen_profiles.h"
#include "ge_character_data.h"
#include "filesystem.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define GES_SS_PROFILE_FILE "cfg/ges_splitscreen_profiles.txt"

static CGESSProfiles g_GESSProfiles;
CGESSProfiles *GESSProfiles() { return &g_GESSProfiles; }

GESSProfile_t::GESSProfile_t()
{
	Q_strncpy( name, "Player", sizeof( name ) );
	Q_strncpy( character, CHAR_RANDOM_IDENT, sizeof( character ) );
	sensitivity = 1.0f;
	invertY = false;
}

const GESSProfile_t &CGESSProfiles::DefaultProfile()
{
	static GESSProfile_t s_Default;
	return s_Default;
}

void CGESSProfiles::Load()
{
	m_Profiles.RemoveAll();

	KeyValues *pKV = new KeyValues( "SplitScreenProfiles" );
	if ( pKV->LoadFromFile( filesystem, GES_SS_PROFILE_FILE, "MOD" ) )
	{
		for ( KeyValues *pProfile = pKV->GetFirstTrueSubKey(); pProfile; pProfile = pProfile->GetNextTrueSubKey() )
		{
			if ( m_Profiles.Count() >= GES_SS_MAX_PROFILES )
				break;

			GESSProfile_t p;
			Q_strncpy( p.name, pProfile->GetString( "name", "Player" ), sizeof( p.name ) );
			Q_strncpy( p.character, pProfile->GetString( "character", CHAR_RANDOM_IDENT ), sizeof( p.character ) );
			p.sensitivity = clamp( pProfile->GetFloat( "sensitivity", 1.0f ), GES_SS_SENS_MIN, GES_SS_SENS_MAX );
			p.invertY = pProfile->GetInt( "invert_y", 0 ) != 0;
			m_Profiles.AddToTail( p );
		}
	}
	pKV->deleteThis();
}

void CGESSProfiles::Save()
{
	KeyValues *pKV = new KeyValues( "SplitScreenProfiles" );

	for ( int i = 0; i < m_Profiles.Count(); i++ )
	{
		const GESSProfile_t &p = m_Profiles[i];
		KeyValues *pProfile = pKV->CreateNewKey();
		pProfile->SetString( "name", p.name );
		pProfile->SetString( "character", p.character );
		pProfile->SetFloat( "sensitivity", p.sensitivity );
		pProfile->SetInt( "invert_y", p.invertY ? 1 : 0 );
	}

	pKV->SaveToFile( filesystem, GES_SS_PROFILE_FILE, "MOD" );
	pKV->deleteThis();
}

int CGESSProfiles::Add( const char *name )
{
	if ( m_Profiles.Count() >= GES_SS_MAX_PROFILES )
		return -1;

	GESSProfile_t p;
	if ( name && name[0] )
		Q_strncpy( p.name, name, sizeof( p.name ) );
	else
		Q_snprintf( p.name, sizeof( p.name ), "Player %d", m_Profiles.Count() + 1 );

	return m_Profiles.AddToTail( p );
}

void CGESSProfiles::Remove( int i )
{
	if ( m_Profiles.IsValidIndex( i ) )
		m_Profiles.Remove( i );
}

