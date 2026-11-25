///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Shared (GES)
//   File        : ge_customchar.cpp
//   Description :
//      see ge_customchar.h
//
////////////////////////////////////////////////////////////////////////////
#include "cbase.h"
#include "ge_customchar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void GECustomChar_t::Clear()
{
	body[0] = head[0] = headFile[0] = '\0';
	bodySkin = headSkin = 0;
}

bool GECustomChar_IsCustom( const char *ident )
{
	if ( !ident )
		return false;
	const int len = Q_strlen( CHAR_CUSTOM_IDENT );
	return !Q_strnicmp( ident, CHAR_CUSTOM_IDENT, len ) && ( ident[len] == '\0' || ident[len] == '/' );
}

static bool IsUsableChar( const CGECharData *pChar )
{
	return pChar && pChar != GECharacters()->GetRandomChar() && Q_stricmp( pChar->szIdentifier, CHAR_RANDOM_IDENT ) && pChar->m_pSkins.Count() > 0;
}

bool GECustomChar_Parse( const char *spec, GECustomChar_t &out )
{
	out.Clear();
	if ( !GECustomChar_IsCustom( spec ) )
		return false;

	// Split on '/'
	char buf[GE_CUSTOMCHAR_SPEC_LEN];
	Q_strncpy( buf, spec, sizeof( buf ) );
	const char *parts[6] = { NULL };
	int n = 0;
	char *p = buf;
	while ( p && n < 6 )
	{
		parts[n++] = p;
		char *slash = strchr( p, '/' );
		if ( slash )
		{
			*slash = '\0';
			p = slash + 1;
		}
		else
			p = NULL;
	}

	// "_custom" on its own: the first character, own head
	const CGECharData *pBody = n > 1 ? GECharacters()->Get( parts[1] ) : NULL;
	if ( !IsUsableChar( pBody ) )
	{
		pBody = NULL;
		for ( int i = 0; i < GECharacters()->Count() && !pBody; i++ )
		{
			if ( IsUsableChar( GECharacters()->Get( i ) ) )
				pBody = GECharacters()->Get( i );
		}
		if ( !pBody )
			return false;
	}

	Q_strncpy( out.body, pBody->szIdentifier, sizeof( out.body ) );
	out.bodySkin = n > 2 ? clamp( atoi( parts[2] ), 0, pBody->m_pSkins.Count() - 1 ) : 0;

	if ( n > 3 && parts[3][0] == '@' )
	{
		GECustomChar_CleanHeadName( parts[3] + 1, out.headFile, sizeof( out.headFile ) );
	}
	else
	{
		const CGECharData *pHead = n > 3 ? GECharacters()->Get( parts[3] ) : NULL;
		if ( !IsUsableChar( pHead ) )
			pHead = pBody;
		Q_strncpy( out.head, pHead->szIdentifier, sizeof( out.head ) );
		out.headSkin = n > 4 ? clamp( atoi( parts[4] ), 0, pHead->m_pSkins.Count() - 1 ) : ( pHead == pBody ? out.bodySkin : 0 );
	}

	return true;
}

void GECustomChar_Format( const GECustomChar_t &c, char *out, int len )
{
	if ( c.IsImportedHead() )
		Q_snprintf( out, len, "%s/%s/%d/@%s", CHAR_CUSTOM_IDENT, c.body, c.bodySkin, c.headFile );
	else
		Q_snprintf( out, len, "%s/%s/%d/%s/%d", CHAR_CUSTOM_IDENT, c.body, c.bodySkin, c.head[0] ? c.head : c.body, c.headSkin );
}

const CGECharData *GECustomChar_GetBodyChar( const char *identOrSpec )
{
	if ( !identOrSpec )
		return NULL;
	if ( !GECustomChar_IsCustom( identOrSpec ) )
		return GECharacters()->Get( identOrSpec );

	GECustomChar_t c;
	if ( !GECustomChar_Parse( identOrSpec, c ) )
		return NULL;
	return GECharacters()->Get( c.body );
}

void GECustomChar_CleanHeadName( const char *in, char *out, int len )
{
	int o = 0;
	const int maxLen = Min( len, GE_CUSTOMHEAD_NAME_LEN ) - 1;
	for ( const char *p = in; p && *p && o < maxLen; p++ )
	{
		char ch = *p;
		if ( ch >= 'A' && ch <= 'Z' )
			ch = ch - 'A' + 'a';
		if ( ( ch >= 'a' && ch <= 'z' ) || ( ch >= '0' && ch <= '9' ) || ch == '_' || ch == '-' )
			out[o++] = ch;
		else if ( ch == ' ' || ch == '.' )
			out[o++] = '_';
	}
	out[o] = '\0';
}

const CGECharData *GECustomChar_RosterEntry()
{
	static CGECharData *s_pCustom = NULL;
	if ( !s_pCustom )
	{
		s_pCustom = new CGECharData;
		Q_strncpy( s_pCustom->szIdentifier, CHAR_CUSTOM_IDENT, sizeof( s_pCustom->szIdentifier ) );
		Q_strncpy( s_pCustom->szPrintName, "Custom", sizeof( s_pCustom->szPrintName ) );
		Q_strncpy( s_pCustom->szShortName, "Custom", sizeof( s_pCustom->szShortName ) );
		Q_strncpy( s_pCustom->szBio, "Build your own agent: put any character's head on any character's body, or import your own head (an .obj file or a compiled .mdl).", sizeof( s_pCustom->szBio ) );
		s_pCustom->iTeam = TEAM_UNASSIGNED;
	}
	return s_pCustom;
}
