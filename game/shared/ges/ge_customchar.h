///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Shared (GES)
//   File        : ge_customchar.h
//   Description :
//      The "Custom" character: any character's body with any character's head,
//      or a head the player imported (models/ges_custom_heads/<name>.mdl).
//
//      A custom choice travels everywhere a character identifier does
//      (joinclass, split screen profiles and commands) as one string:
//
//          _custom/<body ident>/<body skin>/<head ident>/<head skin>
//          _custom/<body ident>/<body skin>/@<imported head name>
//
//      The server plays the body as a normal character and networks the head.
//      Clients hide the body's own head and draw the chosen head on top
//      (ge_customhead.cpp).
//
////////////////////////////////////////////////////////////////////////////
#ifndef GE_CUSTOMCHAR_H
#define GE_CUSTOMCHAR_H
#ifdef _WIN32
#pragma once
#endif

#include "ge_character_data.h"

#define CHAR_CUSTOM_IDENT			"_custom"
#define GE_CUSTOMCHAR_SPEC_LEN		96		// Fits any spec (idents are < 32 chars)
#define GE_CUSTOMHEAD_NAME_LEN		40		// Imported head file name, without extension
#define GE_CUSTOMHEAD_DIR			"models/ges_custom_heads"
#define GE_CUSTOMHEAD_BONE			"ValveBiped.Bip01_Head1"

struct GECustomChar_t
{
	GECustomChar_t() { Clear(); }
	void Clear();

	char	body[MAX_CHAR_IDENT];
	int		bodySkin;
	char	head[MAX_CHAR_IDENT];				// Character whose head is used ("" when imported)
	int		headSkin;
	char	headFile[GE_CUSTOMHEAD_NAME_LEN];	// Imported head ("" when it's a character's)

	bool IsImportedHead() const { return headFile[0] != '\0'; }
};

// True for "_custom" and any "_custom/..." spec
bool GECustomChar_IsCustom( const char *ident );

// Splits a spec. Returns false when it isn't a usable custom spec (unknown body).
// A missing or unknown head falls back to the body's own head.
bool GECustomChar_Parse( const char *spec, GECustomChar_t &out );

// Builds a spec string
void GECustomChar_Format( const GECustomChar_t &c, char *out, int len );

// The character data used for the body of a spec (or the character itself
// for a normal identifier). NULL when unknown.
const CGECharData *GECustomChar_GetBodyChar( const char *identOrSpec );

// Keeps [a-z0-9_-] (lower case), at most GE_CUSTOMHEAD_NAME_LEN - 1 chars
void GECustomChar_CleanHeadName( const char *in, char *out, int len );

// The roster entry for "Custom" (not part of GECharacters())
const CGECharData *GECustomChar_RosterEntry();

#endif // GE_CUSTOMCHAR_H
